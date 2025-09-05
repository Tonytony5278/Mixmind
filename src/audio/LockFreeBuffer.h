#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cassert>

namespace mixmind::audio {

// Lock-free circular buffer for real-time audio processing
template<typename T>
class LockFreeRingBuffer {
public:
    explicit LockFreeRingBuffer(size_t capacity) 
        : capacity_(nextPowerOfTwo(capacity))
        , mask_(capacity_ - 1)
        , buffer_(std::make_unique<T[]>(capacity_))
        , readPos_(0)
        , writePos_(0) {
        
        assert(isPowerOfTwo(capacity_));
    }
    
    ~LockFreeRingBuffer() = default;
    
    // Non-copyable, non-movable for thread safety
    LockFreeRingBuffer(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer& operator=(const LockFreeRingBuffer&) = delete;
    LockFreeRingBuffer(LockFreeRingBuffer&&) = delete;
    LockFreeRingBuffer& operator=(LockFreeRingBuffer&&) = delete;
    
    // Write operations (producer)
    bool write(const T& item) {
        const size_t currentWrite = writePos_.load(std::memory_order_relaxed);
        const size_t nextWrite = (currentWrite + 1) & mask_;
        
        if (nextWrite == readPos_.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }
        
        buffer_[currentWrite] = item;
        writePos_.store(nextWrite, std::memory_order_release);
        return true;
    }
    
    bool write(const T* items, size_t count) {
        if (count == 0) return true;
        
        const size_t available = getWriteAvailable();
        if (count > available) {
            return false; // Not enough space
        }
        
        const size_t currentWrite = writePos_.load(std::memory_order_relaxed);
        const size_t readPos = readPos_.load(std::memory_order_acquire);
        
        // Check if we need to wrap around
        const size_t toEnd = capacity_ - currentWrite;
        if (count <= toEnd) {
            // Single copy
            std::memcpy(&buffer_[currentWrite], items, count * sizeof(T));
        } else {
            // Split copy
            std::memcpy(&buffer_[currentWrite], items, toEnd * sizeof(T));
            std::memcpy(&buffer_[0], items + toEnd, (count - toEnd) * sizeof(T));
        }
        
        const size_t newWrite = (currentWrite + count) & mask_;
        writePos_.store(newWrite, std::memory_order_release);
        return true;
    }
    
    // Read operations (consumer)
    bool read(T& item) {
        const size_t currentRead = readPos_.load(std::memory_order_relaxed);
        
        if (currentRead == writePos_.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }
        
        item = buffer_[currentRead];
        const size_t nextRead = (currentRead + 1) & mask_;
        readPos_.store(nextRead, std::memory_order_release);
        return true;
    }
    
    bool read(T* items, size_t count) {
        if (count == 0) return true;
        
        const size_t available = getReadAvailable();
        if (count > available) {
            return false; // Not enough data
        }
        
        const size_t currentRead = readPos_.load(std::memory_order_relaxed);
        
        // Check if we need to wrap around
        const size_t toEnd = capacity_ - currentRead;
        if (count <= toEnd) {
            // Single copy
            std::memcpy(items, &buffer_[currentRead], count * sizeof(T));
        } else {
            // Split copy
            std::memcpy(items, &buffer_[currentRead], toEnd * sizeof(T));
            std::memcpy(items + toEnd, &buffer_[0], (count - toEnd) * sizeof(T));
        }
        
        const size_t newRead = (currentRead + count) & mask_;
        readPos_.store(newRead, std::memory_order_release);
        return true;
    }
    
    // Peek operations (non-destructive read)
    bool peek(T& item, size_t offset = 0) const {
        const size_t available = getReadAvailable();
        if (offset >= available) {
            return false;
        }
        
        const size_t currentRead = readPos_.load(std::memory_order_relaxed);
        const size_t readIndex = (currentRead + offset) & mask_;
        item = buffer_[readIndex];
        return true;
    }
    
    // Status queries
    size_t getReadAvailable() const {
        const size_t write = writePos_.load(std::memory_order_acquire);
        const size_t read = readPos_.load(std::memory_order_relaxed);
        return (write - read) & mask_;
    }
    
    size_t getWriteAvailable() const {
        return capacity_ - 1 - getReadAvailable();
    }
    
    bool isEmpty() const {
        return getReadAvailable() == 0;
    }
    
    bool isFull() const {
        return getWriteAvailable() == 0;
    }
    
    size_t getCapacity() const {
        return capacity_;
    }
    
    // Reset buffer (not thread-safe - use only when no other threads are accessing)
    void clear() {
        readPos_.store(0, std::memory_order_relaxed);
        writePos_.store(0, std::memory_order_relaxed);
    }
    
private:
    static bool isPowerOfTwo(size_t n) {
        return n > 0 && (n & (n - 1)) == 0;
    }
    
    static size_t nextPowerOfTwo(size_t n) {
        if (n <= 1) return 2;
        
        size_t power = 1;
        while (power < n) {
            power <<= 1;
        }
        return power;
    }
    
    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<T[]> buffer_;
    
    alignas(64) std::atomic<size_t> readPos_;  // Cache line aligned
    alignas(64) std::atomic<size_t> writePos_; // Cache line aligned
};

// Specialized audio buffer pool for zero-allocation real-time processing
class AudioBufferPool {
public:
    struct AudioBuffer {
        std::unique_ptr<float[]> data;
        size_t capacity;
        size_t channels;
        std::atomic<bool> inUse;
        
        AudioBuffer(size_t cap, size_t ch) 
            : data(std::make_unique<float[]>(cap * ch))
            , capacity(cap)
            , channels(ch)
            , inUse(false) {}
        
        void clear() {
            std::memset(data.get(), 0, capacity * channels * sizeof(float));
        }
        
        float* getChannelData(size_t channel) {
            assert(channel < channels);
            return &data[channel * capacity];
        }
        
        const float* getChannelData(size_t channel) const {
            assert(channel < channels);
            return &data[channel * capacity];
        }
    };
    
    AudioBufferPool(size_t bufferCount, size_t bufferSize, size_t channelCount)
        : bufferSize_(bufferSize)
        , channelCount_(channelCount) {
        
        buffers_.reserve(bufferCount);
        for (size_t i = 0; i < bufferCount; ++i) {
            buffers_.emplace_back(std::make_unique<AudioBuffer>(bufferSize, channelCount));
        }
    }
    
    // Get a free buffer (lock-free)
    AudioBuffer* acquire() {
        for (auto& buffer : buffers_) {
            bool expected = false;
            if (buffer->inUse.compare_exchange_weak(expected, true, std::memory_order_acquire)) {
                buffer->clear(); // Clear for clean state
                return buffer.get();
            }
        }
        return nullptr; // No free buffers available
    }
    
    // Return a buffer to the pool (lock-free)
    void release(AudioBuffer* buffer) {
        if (buffer) {
            buffer->inUse.store(false, std::memory_order_release);
        }
    }
    
    // RAII wrapper for automatic buffer management
    class BufferLease {
    public:
        BufferLease(AudioBufferPool* pool, AudioBuffer* buffer) 
            : pool_(pool), buffer_(buffer) {}
        
        ~BufferLease() {
            if (pool_ && buffer_) {
                pool_->release(buffer_);
            }
        }
        
        BufferLease(const BufferLease&) = delete;
        BufferLease& operator=(const BufferLease&) = delete;
        
        BufferLease(BufferLease&& other) noexcept 
            : pool_(other.pool_), buffer_(other.buffer_) {
            other.pool_ = nullptr;
            other.buffer_ = nullptr;
        }
        
        BufferLease& operator=(BufferLease&& other) noexcept {
            if (this != &other) {
                if (pool_ && buffer_) {
                    pool_->release(buffer_);
                }
                pool_ = other.pool_;
                buffer_ = other.buffer_;
                other.pool_ = nullptr;
                other.buffer_ = nullptr;
            }
            return *this;
        }
        
        AudioBuffer* get() const { return buffer_; }
        AudioBuffer* operator->() const { return buffer_; }
        AudioBuffer& operator*() const { return *buffer_; }
        
        explicit operator bool() const { return buffer_ != nullptr; }
        
    private:
        AudioBufferPool* pool_;
        AudioBuffer* buffer_;
    };
    
    BufferLease acquireLease() {
        return BufferLease(this, acquire());
    }
    
    // Statistics
    size_t getTotalBuffers() const { return buffers_.size(); }
    size_t getBufferSize() const { return bufferSize_; }
    size_t getChannelCount() const { return channelCount_; }
    
    size_t getInUseCount() const {
        size_t count = 0;
        for (const auto& buffer : buffers_) {
            if (buffer->inUse.load(std::memory_order_relaxed)) {
                ++count;
            }
        }
        return count;
    }
    
    size_t getFreeCount() const {
        return getTotalBuffers() - getInUseCount();
    }
    
private:
    std::vector<std::unique_ptr<AudioBuffer>> buffers_;
    size_t bufferSize_;
    size_t channelCount_;
};

// Lock-free message queue for real-time audio thread communication
template<typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(size_t capacity = 1024) 
        : buffer_(capacity) {}
    
    // Producer methods (typically called from UI thread)
    bool enqueue(const T& item) {
        return buffer_.write(item);
    }
    
    bool enqueue(const T* items, size_t count) {
        return buffer_.write(items, count);
    }
    
    // Consumer methods (typically called from audio thread)
    bool dequeue(T& item) {
        return buffer_.read(item);
    }
    
    bool dequeue(T* items, size_t count) {
        return buffer_.read(items, count);
    }
    
    // Status queries
    bool empty() const { return buffer_.isEmpty(); }
    bool full() const { return buffer_.isFull(); }
    size_t size() const { return buffer_.getReadAvailable(); }
    size_t capacity() const { return buffer_.getCapacity(); }
    
    void clear() { buffer_.clear(); }
    
private:
    LockFreeRingBuffer<T> buffer_;
};

// Audio command system for thread-safe parameter changes
struct AudioCommand {
    enum Type {
        SET_PARAMETER,
        SET_BYPASS,
        RESET_STATE,
        LOAD_PRESET,
        CUSTOM
    };
    
    Type type;
    int parameterId;
    float value;
    bool boolValue;
    void* customData;
    
    AudioCommand(Type t, int id, float val) 
        : type(t), parameterId(id), value(val), boolValue(false), customData(nullptr) {}
    
    AudioCommand(Type t, int id, bool val) 
        : type(t), parameterId(id), value(0.0f), boolValue(val), customData(nullptr) {}
    
    AudioCommand(Type t, void* data = nullptr) 
        : type(t), parameterId(0), value(0.0f), boolValue(false), customData(data) {}
};

using AudioCommandQueue = LockFreeQueue<AudioCommand>;

// Real-time safe logging system
class RTLogger {
public:
    enum Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };
    
    struct LogEntry {
        Level level;
        std::chrono::high_resolution_clock::time_point timestamp;
        char message[256];
        
        LogEntry() = default;
        LogEntry(Level l, const char* msg) : level(l) {
            timestamp = std::chrono::high_resolution_clock::now();
            strncpy(message, msg, sizeof(message) - 1);
            message[sizeof(message) - 1] = '\0';
        }
    };
    
    RTLogger(size_t capacity = 1024) : queue_(capacity) {}
    
    // Real-time safe logging (called from audio thread)
    void log(Level level, const char* message) {
        LogEntry entry(level, message);
        queue_.enqueue(entry); // Non-blocking
    }
    
    // Consume logs (called from non-real-time thread)
    bool getNextLogEntry(LogEntry& entry) {
        return queue_.dequeue(entry);
    }
    
    void processLogs() {
        LogEntry entry;
        while (getNextLogEntry(entry)) {
            printLogEntry(entry);
        }
    }
    
    size_t getPendingLogCount() const {
        return queue_.size();
    }
    
private:
    void printLogEntry(const LogEntry& entry) {
        const char* levelStr[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.timestamp.time_since_epoch()).count();
        
        std::printf("[%lld] %s: %s\n", time, levelStr[entry.level], entry.message);
    }
    
    LockFreeQueue<LogEntry> queue_;
};

// Global real-time logger instance
extern RTLogger g_rtLogger;

// Convenience macros for real-time logging
#define RT_LOG_DEBUG(msg) g_rtLogger.log(RTLogger::DEBUG, msg)
#define RT_LOG_INFO(msg) g_rtLogger.log(RTLogger::INFO, msg)
#define RT_LOG_WARNING(msg) g_rtLogger.log(RTLogger::WARNING, msg)
#define RT_LOG_ERROR(msg) g_rtLogger.log(RTLogger::ERROR, msg)

} // namespace mixmind::audio