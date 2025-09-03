#include <iostream>
#include "src/vst3/RealVST3Scanner.h"

int main() {
    mixmind::RealVST3Scanner scanner;
    
    std::cout << "=== VST3 Plugin Scanner Test ===" << std::endl;
    
    // Check for Span
    auto spanResult = scanner.findSpanPlugin();
    if (spanResult.isSuccess()) {
        std::cout << "✅ SPAN FOUND: " << spanResult.getValue().path << std::endl;
    } else {
        std::cout << "❌ Span not found" << std::endl;
    }
    
    // Check for TDR Nova
    auto novaResult = scanner.findTDRNovaPlugin();
    if (novaResult.isSuccess()) {
        std::cout << "✅ TDR NOVA FOUND: " << novaResult.getValue().path << std::endl;
    } else {
        std::cout << "❌ TDR Nova not found" << std::endl;
    }
    
    // Scan all plugins
    auto allResult = scanner.scanSystemPlugins();
    if (allResult.isSuccess()) {
        auto plugins = allResult.getValue();
        std::cout << "📊 TOTAL VST3 PLUGINS: " << plugins.size() << std::endl;
        
        for (const auto& plugin : plugins) {
            std::cout << "  • " << plugin.name << " (" << plugin.path << ")" << std::endl;
        }
    } else {
        std::cout << "❌ No VST3 plugins found" << std::endl;
        scanner.printDownloadInstructions();
    }
    
    return 0;
}
