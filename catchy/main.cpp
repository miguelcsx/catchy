#include "analysis/analyzer.hpp"
#include "parser/parser_factory.hpp"
#include "utils/filesystem.hpp"
#include "utils/git.hpp"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Format.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

using namespace llvm;

// Command line options
static cl::OptionCategory CatchyCategory("Catchy Options");

static cl::opt<std::string> InputPath(
    cl::Positional,
    cl::desc("<input path>"),
    cl::Required,
    cl::cat(CatchyCategory));

static cl::opt<std::string> Language(
    "language",
    cl::desc("Specify the programming language"),
    cl::value_desc("lang"),
    cl::cat(CatchyCategory));

static cl::opt<unsigned> Threshold(
    "threshold",
    cl::desc("Minimum complexity threshold (default: 0)"),
    cl::init(0),
    cl::cat(CatchyCategory));

static cl::opt<std::string> OutputFormat(
    "format",
    cl::desc("Output format (json, toml, text)"),
    cl::init("text"),
    cl::cat(CatchyCategory));

static cl::list<std::string> IgnorePatterns(
    "ignore",
    cl::desc("Patterns to ignore (can be specified multiple times)"),
    cl::cat(CatchyCategory));

static cl::opt<bool> Recursive(
    "recursive",
    cl::desc("Recursively analyze directories"),
    cl::init(false),
    cl::cat(CatchyCategory));

static cl::opt<bool> Verbose(
    "verbose",
    cl::desc("Enable verbose output"),
    cl::init(false),
    cl::cat(CatchyCategory));

void output_results_json(const std::vector<catchy::analysis::AnalysisResult>& results) {
    // Calculate total complexity
    size_t total_complexity = 0;
    for (const auto& result : results) {
        total_complexity += result.complexity;
    }

    outs() << "{\n  \"total_complexity\": " << total_complexity << ",\n"
           << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        outs() << "    {\n"
               << "      \"file\": \"" << result.file_path << "\",\n"
               << "      \"function\": \"" << result.function_name << "\",\n"
               << "      \"complexity\": " << result.complexity << ",\n"
               << "      \"start_line\": " << result.start_line << ",\n"
               << "      \"end_line\": " << result.end_line << ",\n"
               << "      \"language\": \"" << result.language << "\"";
        
        if (!result.factors.empty()) {
            outs() << ",\n      \"factors\": [\n";
            for (size_t j = 0; j < result.factors.size(); ++j) {
                const auto& factor = result.factors[j];
                outs() << "        {\n"
                       << "          \"description\": \"" << factor.description << "\",\n"
                       << "          \"increment\": " << factor.increment << ",\n"
                       << "          \"line_number\": " << factor.line_number << "\n"
                       << "        }" << (j < result.factors.size() - 1 ? "," : "") << "\n";
            }
            outs() << "      ]";
        }
        
        outs() << "\n    }" << (i < results.size() - 1 ? "," : "") << "\n";
    }
    outs() << "  ]\n}\n";
}

void output_results_text(const std::vector<catchy::analysis::AnalysisResult>& results) {
    for (const auto& result : results) {
        outs() << "File: " << result.file_path << "\n"
               << "Function: " << result.function_name << "\n"
               << "Language: " << result.language << "\n"
               << "Lines: " << result.start_line << "-" << result.end_line << "\n"
               << "Complexity: " << result.complexity << "\n";
        
        if (Verbose && !result.factors.empty()) {
            outs() << "Complexity Factors:\n";
            for (const auto& factor : result.factors) {
                outs() << "  - " << factor.description 
                       << " (line " << factor.line_number 
                       << ", +" << factor.increment << ")\n";
            }
        }
        outs() << "\n";
    }
    // Total complexity
    size_t total_complexity = 0;
    for (const auto& result : results) {
        total_complexity += result.complexity;
    }
    outs() << "Total complexity: " << total_complexity << "\n";
}

int main(int argc, char **argv) {
    InitLLVM X(argc, argv);
    
    // Configure spdlog
    if (Verbose) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::warn);
    }
    
    // Parse command line options
    cl::HideUnrelatedOptions(CatchyCategory);
    cl::ParseCommandLineOptions(argc, argv, "Catchy - Cognitive Complexity Analyzer\n");
    
    try {
        // Initialize analyzer
        catchy::analysis::Analyzer analyzer;
        analyzer.set_complexity_threshold(Threshold);
        
        if (!Language.empty()) {
            analyzer.set_language(Language);
        }
        
        if (!IgnorePatterns.empty()) {
            analyzer.set_ignore_patterns(std::vector<std::string>(
                IgnorePatterns.begin(), 
                IgnorePatterns.end()
            ));
        }
        
        // Analyze based on input type
        std::vector<catchy::analysis::AnalysisResult> results;
        
        std::filesystem::path input_path(InputPath.getValue());
        
        if (std::filesystem::is_regular_file(input_path)) {
            if (Verbose) {
                spdlog::info("Analyzing file: {}", input_path.string());
            }
            results = analyzer.analyze_file(input_path.string());
        }
        else if (std::filesystem::is_directory(input_path)) {
            if (catchy::utils::is_git_repo(input_path.string())) {
                if (Verbose) {
                    spdlog::info("Analyzing git repository: {}", input_path.string());
                }
                results = analyzer.analyze_git_repository(input_path.string());
            } else {
                if (Verbose) {
                    spdlog::info("Analyzing directory: {} (recursive: {})", 
                        input_path.string(), Recursive ? "yes" : "no");
                }
                results = analyzer.analyze_directory(input_path.string(), Recursive);
            }
        }
        else {
            spdlog::error("Invalid input path: {}", input_path.string());
            return 1;
        }

        // Output results
        if (OutputFormat == "json") {
            output_results_json(results);
        } else if (OutputFormat == "toml") {
            spdlog::error("TOML output format is not yet supported");
            return 1;
        } else {
            output_results_text(results);
        }
    } catch (const std::exception& e) {
        spdlog::error("An error occurred: {}", e.what());
        return 1;
    }

    return 0;
}
