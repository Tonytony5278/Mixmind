#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  if (argc < 2) { 
    std::cout << R"({"error":"usage","ok":false})"; 
    return 0; 
  }
  
  fs::path dir = argv[1];
  nlohmann::json out; 
  out["ok"] = true; 
  out["plugins"] = nlohmann::json::array();
  
  std::vector<std::string> exts{".vst3", ".VST3"};
  
  try {
    for (auto& p : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
      if (!p.is_regular_file()) continue;
      
      auto ext = p.path().extension().string();
      for (auto& e : exts) {
        if (ext == e) {
          out["plugins"].push_back({{"path", p.path().string()}});
          break;
        }
      }
    }
  } catch (const std::exception& e) {
    out["ok"] = false;
    out["error"] = e.what();
  }
  
  std::cout << out.dump(); 
  return 0;
}