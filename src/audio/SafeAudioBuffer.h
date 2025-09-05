#pragma once

#include "LockFreeBuffer.h"
#include <atomic>
#include <memory>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>

namespace mixmind::audio {

// ============================================================================
// CRITICAL SAFETY: Enhanced Audio Buffer with Overflow Protection
// Adds comprehensive bounds checking, overflow detection, and safety monitoring
// ============================================================================

template<typename T>
class SafeAudioBuffer {
public:
    explicit SafeAudioBuffer(size_t capacity) 
        : capacity_(capacity)
        , buffer_(std::make_unique<T[]>(capacity + GUARD_SIZE * 2)) // Guard zones
        , data_(buffer_.get() + GUARD_SIZE) // Offset past guard zone
        , readPos_(0)
        , writePos_(0)
        , overflowCount_(0)
        , underflowCount_(0)
        , totalWrites_(0)
        , totalReads_(0) {
        
        // Initialize guard zones with sentinel values
        initializeGuardZones();
        
        // Zero the main buffer
        std::memset(data_, 0, capacity_ * sizeof(T));
    }
    
    ~SafeAudioBuffer() {
        // Check for buffer corruption on destruction
        if (!verifyGuardZones()) {
            std::cerr << "🚨 CRITICAL: Buffer corruption detected in SafeAudioBuffer destructor!" << std::endl;
        }
    }
    
    // Non-copyable, non-movable for safety
    SafeAudioBuffer(const SafeAudioBuffer&) = delete;
    SafeAudioBuffer& operator=(const SafeAudioBuffer&) = delete;
    SafeAudioBuffer(SafeAudioBuffer&&) = delete;
    SafeAudioBuffer& operator=(SafeAudioBuffer&&) = delete;
    
    // SAFE WRITE: Comprehensive bounds checking
    bool write(const T* data, size_t samples) {
        if (!data) {
            RT_LOG_ERROR("SafeAudioBuffer::write() - null data pointer");
            return false;
        }
        
        if (samples == 0) {
            return true; // Nothing to write is success
        }
        
        // Check available space FIRST
        size_t available = getAvailableSpace();
        if (samples > available) {
            overflowCount_.fetch_add(1);
            RT_LOG_WARNING("SafeAudioBuffer overflow prevented");
            return false; // Overflow prevented
        }
        
        // Verify guard zones before write
        if (!verifyGuardZones()) {
            RT_LOG_ERROR("Guard zone corruption detected before write");
            return false;
        }
        
        // Safe write with wrap-around handling
        size_t writeIndex = writePos_.load(std::memory_order_relaxed);
        
        if (writeIndex + samples <= capacity_) {
            // Single copy - no wrap around
            std::memcpy(data_ + writeIndex, data, samples * sizeof(T));
        } else {
            // Split copy - handle wrap around
            size_t firstPart = capacity_ - writeIndex;
            size_t secondPart = samples - firstPart;
            
            // Copy to end of buffer
            std::memcpy(data_ + writeIndex, data, firstPart * sizeof(T));
            
            // Copy remaining to beginning
            std::memcpy(data_, data + firstPart, secondPart * sizeof(T));
        }
        
        // Update write position atomically
        size_t newWritePos = (writeIndex + samples) % capacity_;
        writePos_.store(newWritePos, std::memory_order_release);
        
        totalWrites_.fetch_add(1);
        
        // Verify guard zones after write
        if (!verifyGuardZones()) {
            RT_LOG_ERROR("Guard zone corruption detected after write");
            return false;
        }
        
        return true;
    }
    
    // SAFE READ: Comprehensive bounds checking
    bool read(T* data, size_t samples) {
        if (!data) {
            RT_LOG_ERROR("SafeAudioBuffer::read() - null data pointer");
            return false;
        }
        
        if (samples == 0) {
            return true; // Nothing to read is success
        }
        
        // Check available data FIRST
        size_t available = getAvailableData();
        if (samples > available) {
            underflowCount_.fetch_add(1);
            
            // Zero the output buffer to prevent garbage audio
            std::memset(data, 0, samples * sizeof(T));
            
            RT_LOG_WARNING("SafeAudioBuffer underflow - output zeroed");
            return false; // Underflow detected
        }
        
        // Verify guard zones before read
        if (!verifyGuardZones()) {
            RT_LOG_ERROR("Guard zone corruption detected before read");
            std::memset(data, 0, samples * sizeof(T));
            return false;
        }
        
        // Safe read with wrap-around handling
        size_t readIndex = readPos_.load(std::memory_order_relaxed);
        
        if (readIndex + samples <= capacity_) {
            // Single copy - no wrap around
            std::memcpy(data, data_ + readIndex, samples * sizeof(T));
        } else {
            // Split copy - handle wrap around
            size_t firstPart = capacity_ - readIndex;
            size_t secondPart = samples - firstPart;
            
            // Copy from current position to end
            std::memcpy(data, data_ + readIndex, firstPart * sizeof(T));
            
            // Copy remaining from beginning
            std::memcpy(data + firstPart, data_, secondPart * sizeof(T));
        }
        
        // Update read position atomically
        size_t newReadPos = (readIndex + samples) % capacity_;
        readPos_.store(newReadPos, std::memory_order_release);
        
        totalReads_.fetch_add(1);
        
        return true;
    }
    
    // Partial read/write with zero-padding (safer for audio)
    size_t writePartial(const T* data, size_t samples) {
        if (!data || samples == 0) {
            return 0;
        }
        
        size_t available = getAvailableSpace();
        size_t toWrite = std::min(samples, available);
        
        if (toWrite > 0) {
            if (write(data, toWrite)) {
                return toWrite;
            }
        }
        
        return 0; // Nothing written
    }
    
    size_t readPartial(T* data, size_t samples) {
        if (!data || samples == 0) {
            return 0;
        }
        
        size_t available = getAvailableData();
        size_t toRead = std::min(samples, available);
        
        if (toRead > 0) {
            if (read(data, toRead)) {
                return toRead;
            }
        }
        
        // Zero remaining if partial read
        if (toRead < samples) {
            std::memset(data + toRead, 0, (samples - toRead) * sizeof(T));
        }
        
        return toRead;
    }
    
    // SAFE STATUS QUERIES
    size_t getAvailableSpace() const {
        size_t write = writePos_.load(std::memory_order_acquire);
        size_t read = readPos_.load(std::memory_order_relaxed);
        
        if (write >= read) {
            return capacity_ - (write - read) - 1; // -1 to distinguish full from empty
        } else {
            return read - write - 1;
        }
    }
    
    size_t getAvailableData() const {
        size_t write = writePos_.load(std::memory_order_acquire);
        size_t read = readPos_.load(std::memory_order_relaxed);
        
        if (write >= read) {
            return write - read;
        } else {
            return capacity_ - read + write;
        }
    }
    
    bool isEmpty() const {
        return getAvailableData() == 0;
    }
    
    bool isFull() const {
        return getAvailableSpace() == 0;
    }
    
    size_t getCapacity() const {
        return capacity_;
    }
    
    // SAFETY MONITORING
    struct SafetyStats {
        uint64_t overflowCount = 0;
        uint64_t underflowCount = 0;
        uint64_t totalWrites = 0;
        uint64_t totalReads = 0;
        bool guardZonesIntact = true;
        double overflowRate = 0.0;
        double underflowRate = 0.0;
    };
    
    SafetyStats getSafetyStats() const {
        SafetyStats stats;
        stats.overflowCount = overflowCount_.load();
        stats.underflowCount = underflowCount_.load();
        stats.totalWrites = totalWrites_.load();
        stats.totalReads = totalReads_.load();
        stats.guardZonesIntact = const_cast<SafeAudioBuffer*>(this)->verifyGuardZones();
        
        if (stats.totalWrites > 0) {
            stats.overflowRate = static_cast<double>(stats.overflowCount) / stats.totalWrites;
        }
        
        if (stats.totalReads > 0) {
            stats.underflowRate = static_cast<double>(stats.underflowCount) / stats.totalReads;
        }
        
        return stats;
    }
    
    // Reset statistics (not thread-safe)
    void resetStats() {
        overflowCount_.store(0);
        underflowCount_.store(0);
        totalWrites_.store(0);
        totalReads_.store(0);
    }
    
    // Clear buffer (not thread-safe)
    void clear() {
        readPos_.store(0, std::memory_order_relaxed);
        writePos_.store(0, std::memory_order_relaxed);
        std::memset(data_, 0, capacity_ * sizeof(T));
        resetStats();
    }
    
    // Health check (can be called periodically)
    bool isHealthy() const {
        auto stats = getSafetyStats();
        
        // Check for excessive overflow/underflow rates
        bool lowOverflowRate = stats.overflowRate < 0.01;  // < 1%
        bool lowUnderflowRate = stats.underflowRate < 0.01; // < 1%
        
        return stats.guardZonesIntact && lowOverflowRate && lowUnderflowRate;
    }
    
private:
    static constexpr size_t GUARD_SIZE = 64; // 64 elements guard zone
    static constexpr T GUARD_VALUE = static_cast<T>(0xDEADBEEF);
    
    void initializeGuardZones() {
        // Initialize pre-buffer guard zone
        for (size_t i = 0; i < GUARD_SIZE; ++i) {
            buffer_[i] = GUARD_VALUE;
        }
        
        // Initialize post-buffer guard zone
        for (size_t i = GUARD_SIZE + capacity_; i < GUARD_SIZE + capacity_ + GUARD_SIZE; ++i) {
            buffer_[i] = GUARD_VALUE;
        }
    }
    
    bool verifyGuardZones() {
        // Check pre-buffer guard zone
        for (size_t i = 0; i < GUARD_SIZE; ++i) {
            if (buffer_[i] != GUARD_VALUE) {
                RT_LOG_ERROR("Pre-buffer guard zone corrupted");
                return false;
            }
        }
        
        // Check post-buffer guard zone
        for (size_t i = GUARD_SIZE + capacity_; i < GUARD_SIZE + capacity_ + GUARD_SIZE; ++i) {
            if (buffer_[i] != GUARD_VALUE) {
                RT_LOG_ERROR("Post-buffer guard zone corrupted");
                return false;
            }
        }
        
        return true;
    }
    
    const size_t capacity_;
    std::unique_ptr<T[]> buffer_; // Includes guard zones
    T* data_;                     // Points to actual data area
    
    alignas(64) std::atomic<size_t> readPos_;   // Cache line aligned
    alignas(64) std::atomic<size_t> writePos_;  // Cache line aligned
    
    // Safety statistics
    std::atomic<uint64_t> overflowCount_;
    std::atomic<uint64_t> underflowCount_;
    std::atomic<uint64_t> totalWrites_;
    std::atomic<uint64_t> totalReads_;
};

// ============================================================================
// Safe Audio Buffer Pool - RAII with Overflow Protection
// ============================================================================

class SafeAudioBufferPool {
public:
    using FloatBuffer = SafeAudioBuffer<float>;
    
    struct BufferHandle {
        std::unique_ptr<FloatBuffer> buffer;
        std::atomic<bool> inUse{false};
        size_t id;
        std::chrono::steady_clock::time_point lastUsed;
        
        BufferHandle(size_t capacity, size_t bufferId) 
            : buffer(std::make_unique<FloatBuffer>(capacity))
            , id(bufferId) {
            lastUsed = std::chrono::steady_clock::now();
        }
    };
    
    SafeAudioBufferPool(size_t bufferCount, size_t bufferSize) 
        : bufferSize_(bufferSize) {
        
        buffers_.reserve(bufferCount);
        for (size_t i = 0; i < bufferCount; ++i) {
            buffers_.emplace_back(std::make_unique<BufferHandle>(bufferSize, i));
        }
    }
    
    // RAII Buffer Lease with automatic safety monitoring
    class SafeBufferLease {
    public:
        SafeBufferLease(SafeAudioBufferPool* pool, BufferHandle* handle) 
            : pool_(pool), handle_(handle) {}
        
        ~SafeBufferLease() {
            if (pool_ && handle_) {
                // Update usage time
                handle_->lastUsed = std::chrono::steady_clock::now();
                
                // Check buffer health before release
                if (!handle_->buffer->isHealthy()) {
                    RT_LOG_WARNING("Unhealthy buffer returned to pool");
                }
                
                pool_->release(handle_);
            }
        }
        
        SafeBufferLease(const SafeBufferLease&) = delete;
        SafeBufferLease& operator=(const SafeBufferLease&) = delete;
        
        SafeBufferLease(SafeBufferLease&& other) noexcept 
            : pool_(other.pool_), handle_(other.handle_) {
            other.pool_ = nullptr;
            other.handle_ = nullptr;
        }
        
        FloatBuffer* get() const { return handle_ ? handle_->buffer.get() : nullptr; }
        FloatBuffer* operator->() const { return get(); }
        FloatBuffer& operator*() const { return *get(); }
        
        explicit operator bool() const { return handle_ && handle_->buffer; }
        
        size_t getBufferId() const { return handle_ ? handle_->id : 0; }
        
    private:
        SafeAudioBufferPool* pool_;
        BufferHandle* handle_;
    };
    
    SafeBufferLease acquire() {
        for (auto& handle : buffers_) {
            bool expected = false;
            if (handle->inUse.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                // Clear buffer for clean state
                handle->buffer->clear();
                return SafeBufferLease(this, handle.get());
            }
        }
        
        RT_LOG_WARNING("No free buffers in SafeAudioBufferPool");
        return SafeBufferLease(nullptr, nullptr);
    }
    
    void release(BufferHandle* handle) {
        if (handle) {
            handle->inUse.store(false, std::memory_order_release);
        }
    }
    
    // Pool health monitoring
    struct PoolStats {
        size_t totalBuffers = 0;
        size_t inUseBuffers = 0;
        size_t healthyBuffers = 0;
        uint64_t totalOverflows = 0;
        uint64_t totalUnderflows = 0;
        double avgOverflowRate = 0.0;
    };
    
    PoolStats getPoolStats() const {
        PoolStats stats;
        stats.totalBuffers = buffers_.size();
        
        uint64_t totalWrites = 0;
        
        for (const auto& handle : buffers_) {
            if (handle->inUse.load(std::memory_order_relaxed)) {
                stats.inUseBuffers++;
            }
            
            auto bufferStats = handle->buffer->getSafetyStats();
            if (handle->buffer->isHealthy()) {
                stats.healthyBuffers++;
            }
            
            stats.totalOverflows += bufferStats.overflowCount;
            stats.totalUnderflows += bufferStats.underflowCount;
            totalWrites += bufferStats.totalWrites;
        }
        
        if (totalWrites > 0) {
            stats.avgOverflowRate = static_cast<double>(stats.totalOverflows) / totalWrites;
        }
        
        return stats;
    }
    
    bool isPoolHealthy() const {
        auto stats = getPoolStats();
        return (stats.healthyBuffers == stats.totalBuffers) && (stats.avgOverflowRate < 0.01);
    }
    
private:
    std::vector<std::unique_ptr<BufferHandle>> buffers_;
    size_t bufferSize_;
};

// Convenience aliases
using SafeFloatBuffer = SafeAudioBuffer<float>;
using SafeBufferPool = SafeAudioBufferPool;

} // namespace mixmind::audio