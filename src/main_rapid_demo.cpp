#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "ai/rapid/RapidCommandProcessor.h"

using namespace mixmind::rapid;

void printWelcome() {
    std::cout << "\n";
    std::cout << "🎵 MixMind AI - Rapid Development Demo\n";
    std::cout << "=====================================\n";
    std::cout << "AI-Powered DAW with Natural Language Control\n";
    std::cout << "\nAvailable Commands:\n";
    std::cout << "  • 'add reverb to track 1' - Add effects to tracks\n";
    std::cout << "  • 'set volume to 50%' - Set parameters\n";
    std::cout << "  • 'make track 1 louder' - Adjust track characteristics\n";
    std::cout << "  • 'play' / 'stop' - Transport control\n";
    std::cout << "  • 'help' - Show this help\n";
    std::cout << "  • 'quit' - Exit demo\n";
    std::cout << "\nType commands in plain English!\n";
    std::cout << "=====================================\n\n";
}

void runDemo() {
    // Initialize the rapid DAW
    RapidDAW daw;
    if (!daw.initialize(44100, 512)) {
        std::cout << "❌ Failed to initialize audio engine\n";
        return;
    }
    
    std::cout << "✅ Audio engine initialized (44.1kHz, 512 samples)\n";
    
    // Set up a basic project
    daw.addTrack("Drums");
    daw.addTrack("Bass");
    daw.addTrack("Guitar");
    daw.addTrack("Vocals");
    
    std::cout << "✅ Created 4 tracks: Drums, Bass, Guitar, Vocals\n";
    std::cout << "\nReady for commands! Try: 'add reverb to track 4'\n";
    
    // Command loop
    std::string input;
    while (true) {
        std::cout << "\n🎤 MixMind> ";
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        // Handle special commands
        if (input == "quit" || input == "exit") {
            break;
        } else if (input == "help") {
            printWelcome();
            continue;
        } else if (input == "status") {
            std::cout << "📊 DAW Status:\n";
            std::cout << "   Tracks: " << daw.getTrackCount() << "\n";
            for (size_t i = 0; i < daw.getTrackCount(); ++i) {
                RapidTrack* track = daw.getTrack(static_cast<int>(i));
                if (track) {
                    std::cout << "   Track " << (i + 1) << ": " << track->getName() 
                             << " (vol: " << track->getVolume() 
                             << ", effects: " << track->getEffectCount() 
                             << ", muted: " << (track->isMuted() ? "yes" : "no") << ")\n";
                }
            }
            continue;
        } else if (input == "test") {
            std::cout << "🧪 Running audio test...\n";
            daw.processTestBlock();
            std::cout << "✅ Audio processing test completed\n";
            continue;
        }
        
        // Execute AI command
        std::cout << "🤖 Processing: \"" << input << "\"\n";
        std::string result = daw.executeCommand(input);
        
        if (result.find("Error") == 0) {
            std::cout << "❌ " << result << "\n";
            std::cout << "💡 Try 'help' for command examples\n";
        } else {
            std::cout << "✅ " << result << "\n";
        }
    }
    
    std::cout << "\n👋 Thanks for trying MixMind AI!\n";
    std::cout << "The future of music production is intelligent! 🎵\n\n";
}

void runAutomatedDemo() {
    std::cout << "\n🚀 Running Automated Demo...\n";
    std::cout << "=============================\n";
    
    RapidDAW daw;
    if (!daw.initialize()) {
        std::cout << "❌ Failed to initialize\n";
        return;
    }
    
    // Set up project
    daw.addTrack("Lead Synth");
    daw.addTrack("Drums");
    daw.addTrack("Bass");
    
    std::vector<std::string> demoCommands = {
        "add reverb to track 1",
        "add gain to track 2", 
        "set volume to 80%",
        "make track 1 louder",
        "make track 2 punchier",
        "play",
        "stop"
    };
    
    for (const auto& command : demoCommands) {
        std::cout << "\n🎤 Command: \"" << command << "\"\n";
        std::string result = daw.executeCommand(command);
        std::cout << "🤖 Result: " << result << "\n";
        
        // Small delay for dramatic effect
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "\n🎉 Demo completed! The AI successfully controlled:\n";
    std::cout << "   • Track creation and management\n";
    std::cout << "   • Effect processing and routing\n";
    std::cout << "   • Parameter adjustment and automation\n";
    std::cout << "   • Transport control and playback\n";
    std::cout << "\n💫 This is just the beginning of intelligent DAW control!\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--auto") {
            runAutomatedDemo();
        } else {
            printWelcome();
            runDemo();
        }
    } catch (const std::exception& e) {
        std::cerr << "💥 Demo crashed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}