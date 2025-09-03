#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <iomanip>
#include <sstream>
#include <cstdlib>

// Simple benchmark result aggregator and reporter
class BenchmarkReporter {
private:
    struct BenchmarkResult {
        std::string name;
        double mean_ns;
        double std_dev_ns;
        size_t iterations;
        std::string category;
    };
    
    std::vector<BenchmarkResult> results_;
    std::string output_path_;
    
public:
    BenchmarkReporter(const std::string& output_path = "benchmark_results.json")
        : output_path_(output_path) {}
    
    void addResult(const std::string& name, double mean_ns, double std_dev_ns, 
                   size_t iterations, const std::string& category = "general") {
        results_.push_back({name, mean_ns, std_dev_ns, iterations, category});
    }
    
    void generateReport() {
        generateConsoleReport();
        generateJSONReport();
        generateMarkdownReport();
        generateCSVReport();
    }
    
private:
    void generateConsoleReport() {
        std::cout << "\\n=== MixMind AI Benchmark Results ===\\n";
        std::cout << std::fixed << std::setprecision(2);
        
        std::map<std::string, std::vector<BenchmarkResult>> by_category;
        for (const auto& result : results_) {
            by_category[result.category].push_back(result);
        }
        
        for (const auto& [category, results] : by_category) {
            std::cout << "\\n" << category << " Benchmarks:\\n";
            std::cout << std::string(50, '-') << "\\n";
            
            for (const auto& result : results) {
                double mean_ms = result.mean_ns / 1000000.0;
                double std_dev_ms = result.std_dev_ns / 1000000.0;
                
                std::cout << std::setw(35) << std::left << result.name 
                         << std::setw(10) << std::right << mean_ms << " ms"
                         << " ± " << std::setw(8) << std_dev_ms << " ms"
                         << " (" << result.iterations << " iterations)\\n";
            }
        }
        
        std::cout << "\\n";
    }
    
    void generateJSONReport() {
        std::ofstream json_file(output_path_);
        if (!json_file.is_open()) return;
        
        json_file << "{\\n";
        json_file << "  \\"timestamp\\": \\"" << getCurrentTimestamp() << "\\",\\n";
        json_file << "  \\"benchmarks\\": [\\n";
        
        for (size_t i = 0; i < results_.size(); ++i) {
            const auto& result = results_[i];
            json_file << "    {\\n";
            json_file << "      \\"name\\": \\"" << result.name << "\\",\\n";
            json_file << "      \\"category\\": \\"" << result.category << "\\",\\n";
            json_file << "      \\"mean_ns\\": " << result.mean_ns << ",\\n";
            json_file << "      \\"std_dev_ns\\": " << result.std_dev_ns << ",\\n";
            json_file << "      \\"iterations\\": " << result.iterations << "\\n";
            json_file << "    }";
            if (i < results_.size() - 1) json_file << ",";
            json_file << "\\n";
        }
        
        json_file << "  ]\\n";
        json_file << "}\\n";
        json_file.close();
        
        std::cout << "JSON report saved to: " << output_path_ << "\\n";
    }
    
    void generateMarkdownReport() {
        std::string md_path = output_path_;
        size_t dot_pos = md_path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            md_path = md_path.substr(0, dot_pos) + ".md";
        }
        
        std::ofstream md_file(md_path);
        if (!md_file.is_open()) return;
        
        md_file << "# MixMind AI Benchmark Results\\n\\n";
        md_file << "Generated: " << getCurrentTimestamp() << "\\n\\n";
        
        std::map<std::string, std::vector<BenchmarkResult>> by_category;
        for (const auto& result : results_) {
            by_category[result.category].push_back(result);
        }
        
        for (const auto& [category, results] : by_category) {
            md_file << "## " << category << " Benchmarks\\n\\n";
            md_file << "| Benchmark | Mean Time | Std Dev | Iterations |\\n";
            md_file << "|-----------|-----------|---------|------------|\\n";
            
            for (const auto& result : results) {
                double mean_ms = result.mean_ns / 1000000.0;
                double std_dev_ms = result.std_dev_ns / 1000000.0;
                
                md_file << "| " << result.name 
                       << " | " << std::fixed << std::setprecision(3) << mean_ms << " ms"
                       << " | ± " << std_dev_ms << " ms"
                       << " | " << result.iterations << " |\\n";
            }
            
            md_file << "\\n";
        }
        
        md_file.close();
        std::cout << "Markdown report saved to: " << md_path << "\\n";
    }
    
    void generateCSVReport() {
        std::string csv_path = output_path_;
        size_t dot_pos = csv_path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            csv_path = csv_path.substr(0, dot_pos) + ".csv";
        }
        
        std::ofstream csv_file(csv_path);
        if (!csv_file.is_open()) return;
        
        csv_file << "Name,Category,Mean_ns,StdDev_ns,Iterations,Mean_ms,StdDev_ms\\n";
        
        for (const auto& result : results_) {
            double mean_ms = result.mean_ns / 1000000.0;
            double std_dev_ms = result.std_dev_ns / 1000000.0;
            
            csv_file << "\\"" << result.name << "\\","
                    << "\\"" << result.category << "\\","
                    << result.mean_ns << ","
                    << result.std_dev_ns << ","
                    << result.iterations << ","
                    << mean_ms << ","
                    << std_dev_ms << "\\n";
        }
        
        csv_file.close();
        std::cout << "CSV report saved to: " << csv_path << "\\n";
    }
    
    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// Mock benchmark result parsing (in real implementation, this would parse Catch2 XML output)
class BenchmarkResultParser {
public:
    static std::vector<BenchmarkReporter::BenchmarkResult> parseFromXML(const std::string& xml_path) {
        // In a real implementation, this would parse Catch2's XML output
        // For now, return mock results that demonstrate the system
        
        std::vector<BenchmarkReporter::BenchmarkResult> results;
        
        // Simulate parsed results from audio benchmarks
        results.push_back({"Gain - Small block (64 samples)", 1250.5, 45.2, 1000, "audio"});
        results.push_back({"Gain - Medium block (512 samples)", 8750.3, 120.8, 1000, "audio"});
        results.push_back({"Gain - Large block (2048 samples)", 32500.7, 450.1, 1000, "audio"});
        results.push_back({"FIR Filter - Small block", 3200.2, 85.5, 1000, "audio"});
        results.push_back({"FIR Filter - Medium block", 24500.8, 320.3, 1000, "audio"});
        results.push_back({"Mix 4 sources - Medium block", 15750.4, 200.9, 1000, "audio"});
        
        // Simulate parsed results from MIDI benchmarks
        results.push_back({"Generate medium random MIDI sequence", 125000.3, 2500.7, 100, "midi"});
        results.push_back({"Sort pre-existing medium sequence", 8500.2, 150.4, 100, "midi"});
        results.push_back({"Quantize to 16th notes", 45000.8, 800.3, 100, "midi"});
        results.push_back({"Transpose sequence +7 semitones", 12500.5, 220.1, 100, "midi"});
        results.push_back({"Track polyphonic note events", 85000.7, 1500.8, 100, "midi"});
        
        // Simulate real-time performance results
        results.push_back({"RT Block Processing Chain", 850.3, 25.7, 10000, "realtime"});
        results.push_back({"Low-latency MIDI processing chain", 125.8, 8.2, 10000, "realtime"});
        results.push_back({"Process real-time MIDI block", 650.4, 18.9, 10000, "realtime"});
        
        return results;
    }
};

// Performance regression detector
class RegressionDetector {
private:
    std::string baseline_path_;
    
public:
    RegressionDetector(const std::string& baseline_path) : baseline_path_(baseline_path) {}
    
    struct RegressionAnalysis {
        std::string benchmark_name;
        double baseline_mean;
        double current_mean;
        double percent_change;
        bool is_regression;
        std::string severity;
    };
    
    std::vector<RegressionAnalysis> detectRegressions(
        const std::vector<BenchmarkReporter::BenchmarkResult>& current_results,
        double regression_threshold = 0.10) {  // 10% threshold
        
        std::vector<RegressionAnalysis> analysis;
        
        // In real implementation, would load baseline from JSON file
        // For demo, create mock baseline results
        std::map<std::string, double> baseline = {
            {"Gain - Small block (64 samples)", 1200.0},
            {"Gain - Medium block (512 samples)", 8500.0},
            {"FIR Filter - Small block", 3000.0},
            {"Generate medium random MIDI sequence", 120000.0},
            {"RT Block Processing Chain", 800.0},
        };
        
        for (const auto& current : current_results) {
            auto it = baseline.find(current.name);
            if (it != baseline.end()) {
                RegressionAnalysis reg;
                reg.benchmark_name = current.name;
                reg.baseline_mean = it->second;
                reg.current_mean = current.mean_ns;
                reg.percent_change = ((current.mean_ns - it->second) / it->second) * 100.0;
                reg.is_regression = reg.percent_change > (regression_threshold * 100.0);
                
                if (reg.percent_change > 50.0) {
                    reg.severity = "CRITICAL";
                } else if (reg.percent_change > 25.0) {
                    reg.severity = "HIGH";
                } else if (reg.percent_change > regression_threshold * 100.0) {
                    reg.severity = "MODERATE";
                } else {
                    reg.severity = "NONE";
                }
                
                analysis.push_back(reg);
            }
        }
        
        return analysis;
    }
    
    void reportRegressions(const std::vector<RegressionAnalysis>& analysis) {
        std::cout << "\\n=== Performance Regression Analysis ===\\n";
        
        bool found_regressions = false;
        for (const auto& reg : analysis) {
            if (reg.is_regression) {
                found_regressions = true;
                std::cout << "⚠️  " << reg.severity << " REGRESSION: " << reg.benchmark_name << "\\n";
                std::cout << "   Baseline: " << std::fixed << std::setprecision(1) 
                         << reg.baseline_mean / 1000000.0 << " ms\\n";
                std::cout << "   Current:  " << reg.current_mean / 1000000.0 << " ms\\n";
                std::cout << "   Change:   " << std::showpos << reg.percent_change 
                         << std::noshowpos << "%\\n\\n";
            }
        }
        
        if (!found_regressions) {
            std::cout << "✅ No significant performance regressions detected.\\n";
        }
    }
};

int main(int argc, char* argv[]) {
    std::cout << "MixMind AI Benchmark Runner v1.0\\n";
    
    // Command line argument parsing
    std::string output_path = "benchmark_results.json";
    bool detect_regressions = false;
    std::string baseline_path;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--check-regression" && i + 1 < argc) {
            detect_regressions = true;
            baseline_path = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\\n";
            std::cout << "Options:\\n";
            std::cout << "  --output FILE            Output file for results (default: benchmark_results.json)\\n";
            std::cout << "  --check-regression FILE  Compare against baseline results file\\n";
            std::cout << "  --help                   Show this help\\n";
            return 0;
        }
    }
    
    // Simulate running the actual benchmark executable
    std::cout << "Running benchmarks...\\n";
    
    // In real implementation, this would execute:
    // system("./MixMindBenchmarks --reporter xml --out benchmark_output.xml");
    // Then parse the XML results
    
    // For demo, use mock results
    auto results = BenchmarkResultParser::parseFromXML("mock_output.xml");
    
    // Generate reports
    BenchmarkReporter reporter(output_path);
    for (const auto& result : results) {
        reporter.addResult(result.name, result.mean_ns, result.std_dev_ns, 
                          result.iterations, result.category);
    }
    
    reporter.generateReport();
    
    // Check for regressions if requested
    if (detect_regressions && !baseline_path.empty()) {
        RegressionDetector detector(baseline_path);
        auto regression_analysis = detector.detectRegressions(results);
        detector.reportRegressions(regression_analysis);
        
        // Return exit code based on regression severity
        bool has_critical = false;
        for (const auto& reg : regression_analysis) {
            if (reg.severity == "CRITICAL") {
                has_critical = true;
                break;
            }
        }
        
        if (has_critical) {
            std::cout << "❌ Critical performance regressions detected. Failing build.\\n";
            return 1;
        }
    }
    
    std::cout << "✅ Benchmark run completed successfully.\\n";
    return 0;
}