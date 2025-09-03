#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <atomic>

// Fixed-size block pool for RT use (no dynamic alloc in callback)
class RtAllocator {
public:
  RtAllocator(size_t blockSize, size_t blocks)
  : blockSize_(blockSize), buf_(blockSize * blocks), free_(blocks) {
    for (size_t i = 0; i < blocks; ++i) free_[i].store(1, std::memory_order_relaxed);
  }
  
  void* try_alloc() noexcept {
    for (size_t i = 0; i < free_.size(); ++i) {
      int expected = 1;
      if (free_[i].compare_exchange_strong(expected, 0, std::memory_order_acq_rel)) {
        return buf_.data() + i * blockSize_;
      }
    }
    return nullptr;
  }
  
  void free(void* p) noexcept {
    auto base = buf_.data();
    auto idx  = (static_cast<uint8_t*>(p) - base) / blockSize_;
    free_[idx].store(1, std::memory_order_release);
  }
  
private:
  const size_t blockSize_;
  std::vector<uint8_t> buf_;
  std::vector<std::atomic<int>> free_;
};