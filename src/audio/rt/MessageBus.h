#pragma once
#include "SpscRing.h"
#include "Messages.h"

namespace mixmind::rt {
  class MessageBus {
  public:
    MessageBus() : toAudio_(1024), toUI_(1024) {}
    
    bool sendToAudio(const msg::Command& cmd) { return toAudio_.push(cmd); }
    bool sendToUI(const msg::Command& cmd)    { return toUI_.push(cmd); }
    
    std::optional<msg::Command> pollAudio()   { return toAudio_.pop(); }
    std::optional<msg::Command> pollUI()      { return toUI_.pop(); }
    
  private:
    SpscRing<msg::Command> toAudio_, toUI_;
  };
}