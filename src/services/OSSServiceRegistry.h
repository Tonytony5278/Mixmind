#pragma once

#include "IOSSService.h"
#include "../core/types.h"
#include "../core/result.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>

namespace mixmind::services {

// ============================================================================
// OSS Service Registry Implementation
// ============================================================================

class OSSServiceRegistry : public IOSSServiceRegistry {
public:
    OSSServiceRegistry();
    ~OSSServiceRegistry() override;
    
    // Non-copyable, movable
    OSSServiceRegistry(const OSSServiceRegistry&) = delete;
    OSSServiceRegistry& operator=(const OSSServiceRegistry&) = delete;
    OSSServiceRegistry(OSSServiceRegistry&&) = default;
    OSSServiceRegistry& operator=(OSSServiceRegistry&&) = default;
    
    // ========================================================================
    // IOSSServiceRegistry Implementation
    // ========================================================================
    
    core::VoidResult registerService(const std::string& serviceName, std::shared_ptr<IOSSService> service) override;
    core::VoidResult unregisterService(const std::string& serviceName) override;
    std::shared_ptr<IOSSService> getService(const std::string& serviceName) const override;
    std::vector<std::string> getAllServiceNames() const override;
    core::AsyncResult<core::VoidResult> initializeAllServices() override;
    core::AsyncResult<core::VoidResult> shutdownAllServices() override;
    bool areAllServicesHealthy() const override;
    RegistryStats getRegistryStats() const override;
    
    // ========================================================================
    // Additional Registry Methods
    // ========================================================================
    
    /// Get typed service (with casting)
    template<typename T>
    std::shared_ptr<T> getTypedService(const std::string& serviceName) const;
    
    /// Register service with automatic initialization
    core::AsyncResult<core::VoidResult> registerAndInitializeService(
        const std::string& serviceName, 
        std::shared_ptr<IOSSService> service
    );
    
    /// Get services by type
    template<typename T>
    std::vector<std::shared_ptr<T>> getServicesByType() const;
    
    /// Check if service exists
    bool hasService(const std::string& serviceName) const;
    
    /// Get service count
    size_t getServiceCount() const;
    
    /// Initialize specific service
    core::AsyncResult<core::VoidResult> initializeService(const std::string& serviceName);
    
    /// Shutdown specific service
    core::AsyncResult<core::VoidResult> shutdownService(const std::string& serviceName);
    
    /// Get service initialization order
    std::vector<std::string> getInitializationOrder() const;
    
    /// Set service initialization order (for dependencies)
    void setInitializationOrder(const std::vector<std::string>& order);
    
    // ========================================================================
    // Event Notifications
    // ========================================================================
    
    enum class RegistryEvent {
        ServiceRegistered,
        ServiceUnregistered,
        ServiceInitialized,
        ServiceShutdown,
        ServiceHealthChanged,
        AllServicesInitialized,
        AllServicesShutdown
    };
    
    using RegistryEventCallback = std::function<void(RegistryEvent event, const std::string& serviceName)>;
    
    /// Add event listener
    void addEventListener(RegistryEventCallback callback);
    
    /// Remove event listener
    void removeEventListener(RegistryEventCallback callback);
    
    // ========================================================================
    // Service Dependencies
    // ========================================================================
    
    /// Add service dependency (serviceB depends on serviceA)
    core::VoidResult addServiceDependency(const std::string& serviceA, const std::string& serviceB);
    
    /// Remove service dependency
    core::VoidResult removeServiceDependency(const std::string& serviceA, const std::string& serviceB);
    
    /// Get service dependencies
    std::vector<std::string> getServiceDependencies(const std::string& serviceName) const;
    
    /// Check if service has dependencies
    bool hasServiceDependencies(const std::string& serviceName) const;
    
    // ========================================================================
    // Configuration Management
    // ========================================================================
    
    /// Configure all services from config map
    core::VoidResult configureAllServices(const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& config);
    
    /// Get configuration for all services
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> getAllConfigurations() const;
    
    /// Save registry state to file
    core::VoidResult saveConfiguration(const std::string& filePath) const;
    
    /// Load registry state from file
    core::AsyncResult<core::VoidResult> loadConfiguration(const std::string& filePath);
    
    // ========================================================================
    // Performance and Monitoring
    // ========================================================================
    
    struct AggregateMetrics {
        double totalAverageProcessingTime = 0.0;
        double totalPeakProcessingTime = 0.0;
        int64_t totalOperations = 0;
        int64_t totalFailedOperations = 0;
        size_t totalMemoryUsage = 0;
        double totalCPUUsage = 0.0;
    };
    
    /// Get aggregate performance metrics from all services
    AggregateMetrics getAggregateMetrics() const;
    
    /// Reset performance metrics for all services
    void resetAllPerformanceMetrics();
    
    /// Run self-test on all services
    core::AsyncResult<std::unordered_map<std::string, bool>> runAllSelfTests();
    
    // ========================================================================
    // Singleton Access (Optional)
    // ========================================================================
    
    /// Get global registry instance
    static std::shared_ptr<OSSServiceRegistry> getInstance();
    
    /// Set global registry instance
    static void setInstance(std::shared_ptr<OSSServiceRegistry> instance);
    
protected:
    // ========================================================================
    // Internal Implementation
    // ========================================================================
    
    /// Notify event listeners
    void notifyEventListeners(RegistryEvent event, const std::string& serviceName);
    
    /// Resolve initialization order based on dependencies
    std::vector<std::string> resolveDependencyOrder() const;
    
    /// Check for circular dependencies
    bool hasCircularDependencies() const;
    
    /// Topological sort for dependency resolution
    std::vector<std::string> topologicalSort() const;
    
private:
    // Service storage
    std::unordered_map<std::string, std::shared_ptr<IOSSService>> services_;
    mutable std::shared_mutex servicesMutex_;
    
    // Dependencies
    std::unordered_map<std::string, std::vector<std::string>> dependencies_;
    std::mutex dependenciesMutex_;
    
    // Initialization order
    std::vector<std::string> initializationOrder_;
    mutable std::mutex orderMutex_;
    
    // Event callbacks
    std::vector<RegistryEventCallback> eventCallbacks_;
    std::mutex callbackMutex_;
    
    // Registry state
    std::atomic<bool> isInitializing_{false};
    std::atomic<bool> isShuttingDown_{false};
    
    // Global instance
    static std::shared_ptr<OSSServiceRegistry> globalInstance_;
    static std::mutex instanceMutex_;
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
inline std::shared_ptr<T> OSSServiceRegistry::getTypedService(const std::string& serviceName) const {
    auto service = getService(serviceName);
    return std::dynamic_pointer_cast<T>(service);
}

template<typename T>
inline std::vector<std::shared_ptr<T>> OSSServiceRegistry::getServicesByType() const {
    std::shared_lock<std::shared_mutex> lock(servicesMutex_);
    std::vector<std::shared_ptr<T>> typedServices;
    
    for (const auto& [name, service] : services_) {
        if (auto typedService = std::dynamic_pointer_cast<T>(service)) {
            typedServices.push_back(typedService);
        }
    }
    
    return typedServices;
}

// ============================================================================
// Convenience Macros for Service Registration
// ============================================================================

#define REGISTER_OSS_SERVICE(registry, serviceName, serviceClass, ...) \
    do { \
        auto service = std::make_shared<serviceClass>(__VA_ARGS__); \
        auto result = registry->registerService(serviceName, service); \
        if (!result.isSuccess()) { \
            throw std::runtime_error("Failed to register service '" + serviceName + "': " + result.getErrorMessage()); \
        } \
    } while(0)

#define REGISTER_AND_INIT_OSS_SERVICE(registry, serviceName, serviceClass, ...) \
    do { \
        auto service = std::make_shared<serviceClass>(__VA_ARGS__); \
        auto result = registry->registerAndInitializeService(serviceName, service); \
        if (!result.get().isSuccess()) { \
            throw std::runtime_error("Failed to register and initialize service '" + serviceName + "'"); \
        } \
    } while(0)

#define GET_OSS_SERVICE(registry, serviceName, serviceType) \
    registry->getTypedService<serviceType>(serviceName)

} // namespace mixmind::services