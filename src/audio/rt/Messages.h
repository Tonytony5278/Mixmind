#pragma once
#include <variant>
#include <string>
#include <vector>

namespace mixmind::msg {
  struct SetTempo { double bpm; };
  struct AddTrack { std::string name; };
  struct InsertPlugin { int trackIdx; std::string pluginId; };

  using Command = std::variant<SetTempo, AddTrack, InsertPlugin>;
}