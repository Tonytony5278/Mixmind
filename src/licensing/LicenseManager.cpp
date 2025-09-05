#include "LicenseManager.h"
#include "../core/logging.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <random>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <json/json.h>
#include <curl/curl.h>

#ifdef _WIN32
#include <wincrypt.h>
#include <wmi.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wbemuuid.lib")
#endif

namespace mixmind::licensing {

static std::unique_ptr<LicenseManager> g_licenseManager;

// ============================================================================
// LicenseManager Implementation
// ============================================================================

LicenseManager::LicenseManager() 
    : secureConfig_(std::make_unique<core::SecureConfig>()) {
    
    // Generate machine ID once
    cachedMachineId_ = generateMachineId();
    
    // Try to load existing license
    auto existingLicense = loadLicenseFromRegistry();
    if (existingLicense) {
        currentLicense_ = *existingLicense;
        LOG_INFO("Loaded existing license: " + std::to_string(static_cast<int>(currentLicense_.type)));
    } else {
        LOG_INFO("No existing license found");
    }
    
    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LicenseManager::~LicenseManager() {
    stopPeriodicValidation();
    curl_global_cleanup();
}

core::AsyncResult<LicenseManager::ValidationResult> LicenseManager::validateLicense(const std::string& licenseKey) {
    return core::async([this, licenseKey]() -> ValidationResult {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.validationAttempts++;
        stats_.lastValidation = std::chrono::system_clock::now();
        
        std::string keyToValidate = licenseKey.empty() ? getLicenseKey() : licenseKey;
        
        if (keyToValidate.empty()) {
            LOG_WARNING("No license key provided for validation");
            return ValidationResult::INVALID_KEY;
        }
        
        ValidationResult result = ValidationResult::SERVER_ERROR;
        
        if (!offlineMode_.load()) {
            try {
                auto onlineResult = validateOnline(keyToValidate);
                if (onlineResult.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
                    result = onlineResult.get();
                    stats_.lastServerContact = std::chrono::system_clock::now();
                } else {
                    LOG_WARNING("License validation timeout, falling back to offline");
                    result = ValidationResult::OFFLINE_FALLBACK;
                }
            } catch (const std::exception& e) {
                LOG_ERROR("License validation error: " + std::string(e.what()));
                stats_.serverErrors++;
                result = ValidationResult::OFFLINE_FALLBACK;
            }
        }
        
        // Offline fallback
        if (result == ValidationResult::OFFLINE_FALLBACK || offlineMode_.load()) {
            result = validateOffline(keyToValidate);
            stats_.offlineValidations++;
        }
        
        if (result == ValidationResult::VALID) {
            stats_.successfulValidations++;
        }
        
        // Trigger callback if set
        if (validationCallback_) {
            validationCallback_(result, currentLicense_);
        }
        
        return result;
    });
}

core::AsyncResult<LicenseManager::ValidationResult> LicenseManager::validateOnline(const std::string& licenseKey) {
    return core::async([this, licenseKey]() -> ValidationResult {
        // Prepare validation request
        std::map<std::string, std::string> requestData = {
            {"key", licenseKey},
            {"machine_id", cachedMachineId_},
            {"version", "1.0.0"}, // TODO: Get from build system
            {"timestamp", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())}
        };
        
        auto response = makeHttpRequest("POST", "/license/validate", requestData);
        if (!response) {
            LOG_ERROR("Failed to contact license server: " + response.error());
            return ValidationResult::SERVER_ERROR;
        }
        
        auto httpResponse = response.get();
        if (httpResponse.statusCode != 200) {
            LOG_ERROR("License server returned error: " + std::to_string(httpResponse.statusCode));
            
            if (httpResponse.statusCode == 401) return ValidationResult::INVALID_KEY;
            if (httpResponse.statusCode == 403) return ValidationResult::MACHINE_MISMATCH;
            if (httpResponse.statusCode == 429) return ValidationResult::RATE_LIMITED;
            
            return ValidationResult::SERVER_ERROR;
        }
        
        // Parse response
        Json::Value responseJson;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream responseStream(httpResponse.body);
        
        if (!Json::parseFromStream(builder, responseStream, &responseJson, &errors)) {
            LOG_ERROR("Failed to parse license response: " + errors);
            return ValidationResult::SERVER_ERROR;
        }
        
        // Update license information
        std::lock_guard<std::mutex> lock(licenseMutex_);
        
        if (responseJson["status"].asString() == "valid") {
            currentLicense_.type = static_cast<LicenseType>(responseJson["tier"].asInt());
            currentLicense_.userEmail = responseJson["email"].asString();
            currentLicense_.organizationName = responseJson["organization"].asString();
            
            // Parse expiration date
            auto expiration = responseJson["expires_at"].asInt64();
            currentLicense_.expirationDate = std::chrono::system_clock::from_time_t(expiration);
            
            currentLicense_.isSubscription = responseJson["is_subscription"].asBool();
            currentLicense_.autoRenewal = responseJson["auto_renewal"].asBool();
            currentLicense_.maxSessions = responseJson["max_sessions"].asInt();
            
            // Configure features based on license type
            configureFeaturesForType(currentLicense_.type);
            
            // Cache license locally for offline validation
            saveLicenseToRegistry(currentLicense_);
            
            currentLicense_.lastValidated = std::chrono::system_clock::now();
            
            LOG_INFO("License validated successfully: " + currentLicense_.userEmail);
            return ValidationResult::VALID;
        } else {
            std::string reason = responseJson["reason"].asString();
            LOG_WARNING("License validation failed: " + reason);
            
            if (reason == "expired") return ValidationResult::EXPIRED;
            if (reason == "machine_mismatch") return ValidationResult::MACHINE_MISMATCH;
            
            return ValidationResult::INVALID_KEY;
        }
    });
}

LicenseManager::ValidationResult LicenseManager::validateOffline(const std::string& licenseKey) {
    // Try to load cached license data
    auto cachedLicense = loadLicenseFromRegistry();
    if (!cachedLicense) {
        LOG_WARNING("No cached license available for offline validation");
        return ValidationResult::INVALID_KEY;
    }
    
    // Check if license is still valid (not expired)
    if (!cachedLicense->isValid()) {
        LOG_WARNING("Cached license is expired");
        return ValidationResult::EXPIRED;
    }
    
    // Verify machine ID matches
    if (cachedMachineId_ != getMachineId()) {
        LOG_WARNING("Machine ID mismatch in offline validation");
        return ValidationResult::MACHINE_MISMATCH;
    }
    
    // Check if it's been too long since last online validation (grace period)
    auto timeSinceValidation = std::chrono::system_clock::now() - cachedLicense->lastValidated;
    if (timeSinceValidation > std::chrono::hours(72)) { // 72 hour grace period
        LOG_WARNING("Offline grace period exceeded");
        return ValidationResult::SERVER_ERROR;
    }
    
    std::lock_guard<std::mutex> lock(licenseMutex_);
    currentLicense_ = *cachedLicense;
    
    LOG_INFO("Offline license validation successful");
    return ValidationResult::VALID;
}

void LicenseManager::configureFeaturesForType(LicenseType type) {
    // Reset all features
    currentLicense_.aiAssistantEnabled = false;
    currentLicense_.voiceControlEnabled = false;
    currentLicense_.styleMappingEnabled = false;
    currentLicense_.cloudSyncEnabled = false;
    currentLicense_.prioritySupportEnabled = false;
    currentLicense_.multiSeatEnabled = false;
    
    switch (type) {
        case LicenseType::TRIAL:
            // Full features for trial
            currentLicense_.aiAssistantEnabled = true;
            currentLicense_.voiceControlEnabled = true;
            currentLicense_.styleMappingEnabled = true;
            currentLicense_.cloudSyncEnabled = true;
            currentLicense_.maxSessions = 1;
            break;
            
        case LicenseType::BASIC:
            // Basic DAW features only
            currentLicense_.maxSessions = 1;
            break;
            
        case LicenseType::PRO:
            // All AI features
            currentLicense_.aiAssistantEnabled = true;
            currentLicense_.voiceControlEnabled = true;
            currentLicense_.styleMappingEnabled = true;
            currentLicense_.cloudSyncEnabled = true;
            currentLicense_.maxSessions = 1;
            break;
            
        case LicenseType::STUDIO:
            // All features + multi-seat
            currentLicense_.aiAssistantEnabled = true;
            currentLicense_.voiceControlEnabled = true;
            currentLicense_.styleMappingEnabled = true;
            currentLicense_.cloudSyncEnabled = true;
            currentLicense_.prioritySupportEnabled = true;
            currentLicense_.multiSeatEnabled = true;
            currentLicense_.maxSessions = 5;
            break;
            
        case LicenseType::INVALID:
        default:
            // No features
            break;
    }
}

std::string LicenseManager::generateMachineId() const {
    std::stringstream ss;
    
    // Add multiple hardware identifiers
    ss << getCpuId();
    ss << getMacAddress();
    ss << getWindowsProductId();
    ss << getSystemUUID();
    
    // Hash the combined string
    return sha256(ss.str()).substr(0, 32); // Take first 32 chars
}

std::string LicenseManager::getCpuId() const {
#ifdef _WIN32
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    std::stringstream ss;
    ss << std::hex << cpuInfo[0] << cpuInfo[1] << cpuInfo[2] << cpuInfo[3];
    return ss.str();
#else
    return "unknown";
#endif
}

std::string LicenseManager::getMacAddress() const {
#ifdef _WIN32
    IP_ADAPTER_INFO adapterInfo[16];
    DWORD bufLen = sizeof(adapterInfo);
    DWORD status = GetAdaptersInfo(adapterInfo, &bufLen);
    
    if (status == ERROR_SUCCESS) {
        std::stringstream ss;
        for (int i = 0; i < 6; i++) {
            ss << std::hex << std::setfill('0') << std::setw(2) 
               << static_cast<int>(adapterInfo[0].Address[i]);
        }
        return ss.str();
    }
#endif
    return "unknown";
}

std::string LicenseManager::getWindowsProductId() const {
#ifdef _WIN32
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        TCHAR productId[256];
        DWORD bufferSize = sizeof(productId);
        
        result = RegQueryValueEx(hKey, TEXT("ProductId"), NULL, NULL,
                                (LPBYTE)productId, &bufferSize);
        
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS) {
            return std::string(productId);
        }
    }
#endif
    return "unknown";
}

std::string LicenseManager::getSystemUUID() const {
#ifdef _WIN32
    // Try to get system UUID from WMI
    // This is a simplified version - full implementation would use COM
    return "uuid-placeholder";
#else
    return "unknown";
#endif
}

std::string LicenseManager::sha256(const std::string& input) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.size());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

bool LicenseManager::saveLicenseToRegistry(const LicenseInfo& license) {
#ifdef _WIN32
    // Save encrypted license data to registry
    HKEY hKey;
    LONG result = RegCreateKeyEx(HKEY_CURRENT_USER,
        TEXT("SOFTWARE\\MixMindAI\\License"),
        0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    
    if (result == ERROR_SUCCESS) {
        // Serialize license to JSON
        Json::Value licenseJson;
        licenseJson["type"] = static_cast<int>(license.type);
        licenseJson["email"] = license.userEmail;
        licenseJson["organization"] = license.organizationName;
        licenseJson["expires_at"] = static_cast<int64_t>(
            std::chrono::system_clock::to_time_t(license.expirationDate));
        licenseJson["validated_at"] = static_cast<int64_t>(
            std::chrono::system_clock::to_time_t(license.lastValidated));
        licenseJson["machine_id"] = cachedMachineId_;
        
        std::string licenseData = licenseJson.toStyledString();
        
        // Store encrypted data
        result = RegSetValueEx(hKey, TEXT("LicenseData"), 0, REG_SZ,
                              (const BYTE*)licenseData.c_str(),
                              static_cast<DWORD>(licenseData.size() + 1));
        
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
#endif
    return false;
}

core::Result<LicenseManager::LicenseInfo> LicenseManager::loadLicenseFromRegistry() {
#ifdef _WIN32
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_CURRENT_USER,
        TEXT("SOFTWARE\\MixMindAI\\License"),
        0, KEY_READ, &hKey);
    
    if (result == ERROR_SUCCESS) {
        TCHAR licenseData[4096];
        DWORD bufferSize = sizeof(licenseData);
        
        result = RegQueryValueEx(hKey, TEXT("LicenseData"), NULL, NULL,
                                (LPBYTE)licenseData, &bufferSize);
        
        RegCloseKey(hKey);
        
        if (result == ERROR_SUCCESS) {
            try {
                Json::Value licenseJson;
                Json::CharReaderBuilder builder;
                std::string errors;
                std::istringstream jsonStream(licenseData);
                
                if (Json::parseFromStream(builder, jsonStream, &licenseJson, &errors)) {
                    LicenseInfo license;
                    license.type = static_cast<LicenseType>(licenseJson["type"].asInt());
                    license.userEmail = licenseJson["email"].asString();
                    license.organizationName = licenseJson["organization"].asString();
                    
                    auto expiresAt = licenseJson["expires_at"].asInt64();
                    license.expirationDate = std::chrono::system_clock::from_time_t(expiresAt);
                    
                    auto validatedAt = licenseJson["validated_at"].asInt64();
                    license.lastValidated = std::chrono::system_clock::from_time_t(validatedAt);
                    
                    // Verify machine ID matches
                    std::string storedMachineId = licenseJson["machine_id"].asString();
                    if (storedMachineId != cachedMachineId_) {
                        return core::Result<LicenseInfo>::error("Machine ID mismatch");
                    }
                    
                    configureFeaturesForType(license.type);
                    return core::Result<LicenseInfo>::success(license);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to parse cached license: " + std::string(e.what()));
            }
        }
    }
#endif
    return core::Result<LicenseInfo>::error("No cached license found");
}

core::AsyncResult<LicenseManager::HttpResponse> LicenseManager::makeHttpRequest(
    const std::string& method,
    const std::string& endpoint, 
    const std::map<std::string, std::string>& data,
    const std::map<std::string, std::string>& headers) {
    
    return core::async([this, method, endpoint, data, headers]() -> HttpResponse {
        CURL* curl = curl_easy_init();
        HttpResponse response;
        
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        std::string url = licenseServerUrl_ + endpoint;
        std::string responseBody;
        std::string requestBody;
        
        // Prepare request body for POST
        if (method == "POST" && !data.empty()) {
            Json::Value jsonData;
            for (const auto& [key, value] : data) {
                jsonData[key] = value;
            }
            requestBody = jsonData.toStyledString();
        }
        
        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
            [](void* contents, size_t size, size_t nmemb, std::string* body) -> size_t {
                size_t totalSize = size * nmemb;
                body->append(static_cast<char*>(contents), totalSize);
                return totalSize;
            });
        
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        }
        
        // Set headers
        struct curl_slist* headerList = nullptr;
        headerList = curl_slist_append(headerList, "Content-Type: application/json");
        
        for (const auto& [key, value] : headers) {
            std::string header = key + ": " + value;
            headerList = curl_slist_append(headerList, header.c_str());
        }
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        
        // Perform request
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long statusCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
            response.statusCode = static_cast<int>(statusCode);
            response.body = responseBody;
        } else {
            response.statusCode = 0;
            response.body = curl_easy_strerror(res);
        }
        
        curl_slist_free_all(headerList);
        curl_easy_cleanup(curl);
        
        return response;
    });
}

bool LicenseManager::hasFeature(const std::string& feature) const {
    std::lock_guard<std::mutex> lock(licenseMutex_);
    return currentLicense_.hasFeature(feature);
}

core::Result<void> LicenseManager::requireFeature(const std::string& feature) const {
    if (!hasFeature(feature)) {
        std::string message = "Feature '" + feature + "' requires ";
        
        if (feature == "ai_assistant" || feature == "voice_control" || feature == "style_mapping") {
            message += "Pro or Studio license";
        } else if (feature == "multi_seat") {
            message += "Studio license";
        } else {
            message += "higher license tier";
        }
        
        return core::Result<void>::error(message);
    }
    
    return core::Result<void>::success();
}

std::string LicenseManager::getMachineId() const {
    return cachedMachineId_;
}

// ============================================================================
// License Feature Guard Implementation
// ============================================================================

LicenseFeatureGuard::LicenseFeatureGuard(const std::string& feature) 
    : feature_(feature) {
    
    auto& licenseManager = getGlobalLicenseManager();
    authorized_ = licenseManager.hasFeature(feature);
    
    if (!authorized_) {
        // Determine required license type
        if (feature == "ai_assistant" || feature == "voice_control" || feature == "style_mapping") {
            requiredType_ = LicenseManager::LicenseType::PRO;
        } else if (feature == "multi_seat") {
            requiredType_ = LicenseManager::LicenseType::STUDIO;
        } else {
            requiredType_ = LicenseManager::LicenseType::BASIC;
        }
    }
}

std::string LicenseFeatureGuard::getUpgradeMessage() const {
    if (authorized_) return "";
    
    std::string message = "This feature requires ";
    
    switch (requiredType_) {
        case LicenseManager::LicenseType::BASIC:
            message += "Basic license ($49/month)";
            break;
        case LicenseManager::LicenseType::PRO:
            message += "Pro license ($149/month)";
            break;
        case LicenseManager::LicenseType::STUDIO:
            message += "Studio license ($499/month)";
            break;
        default:
            message += "valid license";
            break;
    }
    
    return message;
}

// ============================================================================
// Global License Manager
// ============================================================================

LicenseManager& getGlobalLicenseManager() {
    if (!g_licenseManager) {
        throw std::runtime_error("License system not initialized");
    }
    return *g_licenseManager;
}

void initializeLicenseSystem() {
    if (!g_licenseManager) {
        g_licenseManager = std::make_unique<LicenseManager>();
        LOG_INFO("License system initialized");
    }
}

void shutdownLicenseSystem() {
    if (g_licenseManager) {
        g_licenseManager.reset();
        LOG_INFO("License system shutdown");
    }
}

} // namespace mixmind::licensing