#include "analysis/analyzer.hpp"
#include "parser/parser_factory.hpp"
#include "utils/filesystem.hpp"
#include "utils/git.hpp"
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/raw_ostream.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <vector>
#include <tabulate/table.hpp>

using namespace llvm;
using namespace tabulate;

// Command line options
static cl::OptionCategory CatchyCategory("Catchy Options");

static cl::opt<std::string> InputPath(
    cl::Positional,
    cl::desc("<input path>"),
    cl::Required,
    cl::cat(CatchyCategory));

static cl::opt<unsigned> Threshold(
    "threshold",
    cl::desc("Minimum complexity threshold (default: 0)"),
    cl::init(0),
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

// Function to display results using Tabulate
void display_results(const std::vector<catchy::analysis::AnalysisResult>& results) {
    using catchy::analysis::AnalysisResult;

    Table table;

    // Add headers
    table.add_row({"Path", "File", "Function", "Complexity"});
    table[0].format()
        .font_style({FontStyle::bold})
        .font_align(FontAlign::center)
        .font_background_color(Color::cyan);

    size_t total_complexity = 0;
    std::unordered_map<std::string, size_t> complexity_per_file;

    // Add rows with results
    for (const auto& result : results) {
        std::string file_name = std::filesystem::path(result.file_path).filename().string();
        table.add_row({
            result.file_path,
            file_name,
            result.function_name,
            std::to_string(result.complexity)
        });

        total_complexity += result.complexity;
        complexity_per_file[result.file_path] += result.complexity;
    }

    // Format rows
    for (size_t i = 1; i < table.size(); ++i) {
        table[i].format().font_align(FontAlign::left);
    }

    // Print the table
    std::cout << table << std::endl;

    // Display summary
    std::cout << "\nSummary:\n";
    if (complexity_per_file.size() > 1) {
        std::cout << "Files analyzed: " << complexity_per_file.size() << "\n";
    }
    for (const auto& [file, complexity] : complexity_per_file) {
        std::cout << "Total for file " << file << ": " << complexity << "\n";
    }
    std::cout << "Total complexity for all files: " << total_complexity << "\n";
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

        // Analyze based on input type
        std::vector<catchy::analysis::AnalysisResult> results;
        std::filesystem::path input_path(InputPath.getValue());

        if (std::filesystem::is_regular_file(input_path)) {
            if (Verbose) {
                spdlog::info("Analyzing file: {}", input_path.string());
            }
            results = analyzer.analyze_file(input_path.string());
        } else if (std::filesystem::is_directory(input_path)) {
            if (Verbose) {
                spdlog::info("Analyzing directory: {} (recursive: {})", 
                    input_path.string(), Recursive ? "yes" : "no");
            }
            results = analyzer.analyze_directory(input_path.string(), Recursive);
        } else {
            spdlog::error("Invalid input path: {}", input_path.string());
            return 1;
        }

        // Display results using Tabulate
        display_results(results);
    } catch (const std::exception& e) {
        spdlog::error("An error occurred: {}", e.what());
        return 1;
    }

    return 0;
}
