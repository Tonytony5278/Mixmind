#pragma once
#include <atomic>
#include <cstddef>
#include <optional>

// Single-producer/single-consumer ring buffer (power-of-two capacity)
template <typename T>
class SpscRing {
public:
  explicit SpscRing(size_t capacity_pow2)
  : mask_(capacity_pow2 - 1), buf_(new T[capacity_pow2]) {}

  ~SpscRing() { delete[] buf_; }

  bool push(const T& v) {
    auto head = head_.load(std::memory_order_relaxed);
    auto next = (head + 1) & mask_;
    if (next == tail_.load(std::memory_order_acquire)) return false;
    buf_[head] = v;
    head_.store(next, std::memory_order_release);
    return true;
  }

  std::optional<T> pop() {
    auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) return std::nullopt;
    T v = buf_[tail];
    tail_.store((tail + 1) & mask_, std::memory_order_release);
    return v;
  }

private:
  const size_t mask_;
  T* buf_;
  std::atomic<size_t> head_{0}, tail_{0};
};