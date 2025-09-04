#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("Basic functionality", "[core]") {
    SECTION("Math operations work correctly") {
        REQUIRE(2 + 2 == 4);
        REQUIRE(3.14 == Catch::Approx(3.14).margin(0.01));
    }
    
    SECTION("String operations") {
        std::string test = "MixMind AI";
        REQUIRE(test.length() == 10);
        REQUIRE(test.find("AI") != std::string::npos);
    }
}

TEST_CASE("Audio concepts", "[audio]") {
    SECTION("Sample rate calculations") {
        const int sampleRate = 44100;
        const double duration = 1.0; // 1 second
        const int expectedSamples = static_cast<int>(sampleRate * duration);
        
        REQUIRE(expectedSamples == 44100);
    }
    
    SECTION("Decibel conversions") {
        // Basic audio math that any DAW needs
        auto linearToDb = [](double linear) {
            return 20.0 * std::log10(linear);
        };
        
        REQUIRE(linearToDb(1.0) == Catch::Approx(0.0).margin(0.01));
        REQUIRE(linearToDb(0.5) == Catch::Approx(-6.02).margin(0.01));
    }
}

TEST_CASE("Memory management", "[core]") {
    SECTION("Smart pointers work correctly") {
        auto ptr = std::make_unique<int>(42);
        REQUIRE(*ptr == 42);
        
        auto shared = std::make_shared<std::string>("MixMind");
        REQUIRE(*shared == "MixMind");
    }
    
    SECTION("Vector operations") {
        std::vector<float> audioBuffer(1024, 0.0f);
        REQUIRE(audioBuffer.size() == 1024);
        
        // Fill with test data
        for (size_t i = 0; i < audioBuffer.size(); ++i) {
            audioBuffer[i] = static_cast<float>(std::sin(i * 0.1));
        }
        
        REQUIRE(audioBuffer[0] == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(audioBuffer[100] != 0.0f); // Should have sine wave data
    }
}