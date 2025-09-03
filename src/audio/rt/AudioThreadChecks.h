#pragma once
#include <atomic>
#include <thread>

// Mark the audio thread; cheap "asserts" for locks/allocations are cultural rules
struct AudioThreadGuard {
  static void mark_audio_thread() { id().store(std::this_thread::get_id()); }
  static bool on_audio_thread()   { return id().load() == std::this_thread::get_id(); }
private:
  static std::atomic<std::thread::id>& id() { 
    static std::atomic<std::thread::id> i; 
    return i; 
  }
};

// Usage:
// In audio callback:
//   AudioThreadGuard::mark_audio_thread();
// Then prefer lock-free structures; review prohibits new/malloc on audio thread.