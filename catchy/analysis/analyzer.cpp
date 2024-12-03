#include "analyzer.hpp"
#include "parser/parser_factory.hpp"
#include "parser/languages/cpp_parser.hpp"
#include "utils/safe_conversions.hpp"
#include "utils/filesystem.hpp"
#include "utils/git.hpp"
#include <spdlog/spdlog.h>
#include <llvm/Support/raw_ostream.h>
#include <stack>

namespace catchy::analysis {

Analyzer::Analyzer() 
    : complexity_calculator_(std::make_unique<complexity::CognitiveComplexity>()) 
{
    try {
        // Register available parsers
        parser::ParserFactory::instance().register_parser<parser::languages::CppParser>();
        spdlog::debug("Registered C++ parser");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize parsers: {}", e.what());
        throw;
    }
}

Analyzer::~Analyzer() = default;

std::vector<AnalysisResult> Analyzer::analyze_file(const std::string& file_path) {
    try {
        // Read file content
        std::string content = utils::read_file_content(file_path);
        spdlog::debug("File content:\n{}", content);
        
        // Detect language if not specified
        std::string lang = language_.empty() ? detect_language(file_path) : language_;
        if (lang.empty()) {
            spdlog::error("Could not detect language for file: {}", file_path);
            return {};
        }
        spdlog::debug("Detected language: {}", lang);
        
        return analyze_content(content, file_path, lang);
    } catch (const std::exception& e) {
        spdlog::error("Failed to analyze file {}: {}", file_path, e.what());
        return {};
    }
}

std::vector<AnalysisResult> Analyzer::analyze_directory(
    const std::string& directory_path,
    bool recursive
) {
    std::vector<AnalysisResult> results;
    
    try {
        auto files = utils::list_files(directory_path, recursive);
        
        for (const auto& file : files) {
            if (should_analyze_file(file)) {
                auto file_results = analyze_file(file);
                results.insert(results.end(), file_results.begin(), file_results.end());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to analyze directory {}: {}", directory_path, e.what());
    }
    
    return results;
}

std::vector<AnalysisResult> Analyzer::analyze_git_repository(
    const std::string& repository_path
) {
    std::vector<AnalysisResult> results;
    
    try {
        if (!utils::is_git_repo(repository_path)) {
            throw std::runtime_error("Not a git repository");
        }
        
        auto files = utils::list_git_files(repository_path);
        
        for (const auto& file : files) {
            if (should_analyze_file(file)) {
                auto file_results = analyze_file(file);
                results.insert(results.end(), file_results.begin(), file_results.end());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to analyze git repository {}: {}", repository_path, e.what());
    }
    
    return results;
}

std::string Analyzer::detect_language(const std::string& file_path) const {
    auto parser = parser::ParserFactory::instance().create_parser_for_file(file_path);
    if (parser) {
        auto lang = parser->get_language_name();
        spdlog::debug("Language detected from file extension: {}", lang);
        return lang;
    }
    spdlog::warn("No parser found for file: {}", file_path);
    return "";
}

bool Analyzer::should_analyze_file(const std::string& file_path) const {
    // Check if file matches any ignore patterns
    for (const auto& pattern : ignore_patterns_) {
        if (utils::matches_pattern(file_path, pattern)) {
            return false;
        }
    }
    
    // Check if we have a parser for this file type
    return parser::ParserFactory::instance().create_parser_for_file(file_path) != nullptr;
}

std::vector<AnalysisResult> Analyzer::analyze_content(
    const std::string& content,
    const std::string& file_path,
    const std::string& language
) {
    std::vector<AnalysisResult> results;
    
    auto parser = parser::ParserFactory::instance().create_parser(language);
    if (!parser) {
        spdlog::error("No parser available for language: {}", language);
        return results;
    }
    
    // Initialize parser
    if (!parser->initialize()) {
        spdlog::error("Failed to initialize parser for {}", file_path);
        return results;
    }
    
    // Parse the entire file first
    TSParser* ts_parser = ts_parser_new();
    if (!ts_parser) {
        spdlog::error("Failed to create tree-sitter parser");
        return results;
    }

    // Set language for tree-sitter parser
    if (language == "cpp") {
        ts_parser_set_language(ts_parser, tree_sitter_cpp());
    } else {
        spdlog::error("Unsupported language for tree-sitter: {}", language);
        ts_parser_delete(ts_parser);
        return results;
    }

    // Parse the entire file
    TSTree* full_tree = ts_parser_parse_string(
        ts_parser,
        nullptr,
        content.c_str(),
        utils::safe_string_length(content)
    );

    if (!full_tree) {
        spdlog::error("Failed to parse file: {}", file_path);
        ts_parser_delete(ts_parser);
        return results;
    }

    // Parse functions
    parser::ParserContext context{content, file_path};
    auto functions = parser->parse_functions(context);
    
    spdlog::debug("Found {} functions to analyze", functions.size());
    
    // Analyze each function
    for (const auto& func : functions) {
        spdlog::debug("Analyzing function '{}' (lines {}-{})", 
            func.name, func.start_line, func.end_line);
            
        AnalysisResult result;
        result.file_path = file_path;
        result.language = language;
        result.function_name = func.name;
        result.start_line = func.start_line;
        result.end_line = func.end_line;

        // Extract function node from the full tree
        TSNode root_node = ts_tree_root_node(full_tree);
        TSNode function_node = find_function_node(root_node, func.name, content);

        if (!ts_node_is_null(function_node)) {
            auto complexity_result = complexity_calculator_->calculate(function_node, content);
            
            result.complexity = complexity_result.total_complexity;
            result.factors = std::move(complexity_result.factors);
            
            spdlog::debug("Function '{}' complexity: {}", func.name, result.complexity);
            
            // Only add if complexity is above threshold
            if (result.complexity >= complexity_threshold_) {
                results.push_back(std::move(result));
            }
        } else {
            spdlog::error("Failed to find function node for {}", func.name);
        }
    }
    
    ts_tree_delete(full_tree);
    ts_parser_delete(ts_parser);
    
    return results;
}

TSNode Analyzer::find_function_node(TSNode node, const std::string& function_name, const std::string& source) {
    if (ts_node_is_null(node)) {
        return node;
    }

    const char* type = ts_node_type(node);
    if (strcmp(type, "function_definition") == 0 || strcmp(type, "function_declaration") == 0) {
        // Extract the function name from the node
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", strlen("declarator"));
        if (!ts_node_is_null(declarator)) {
            std::string current_func_name = extract_function_name(declarator, source);
            if (current_func_name == function_name) {
                return node;
            }
        }
    }

    // Recursively search children
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode result = find_function_node(ts_node_child(node, i), function_name, source);
        if (!ts_node_is_null(result)) {
            return result;
        }
    }

    return TSNode{};
}

std::string Analyzer::extract_function_name(TSNode node, const std::string& source) {
    // Find the identifier node
    std::stack<TSNode> nodes;
    nodes.push(node);
    
    while (!nodes.empty()) {
        TSNode current = nodes.top();
        nodes.pop();
        
        if (strcmp(ts_node_type(current), "identifier") == 0) {
            uint32_t start = ts_node_start_byte(current);
            uint32_t end = ts_node_end_byte(current);
            if (start < source.length() && end <= source.length()) {
                return source.substr(start, end - start);
            }
        }
        
        uint32_t child_count = ts_node_child_count(current);
        for (uint32_t i = 0; i < child_count; i++) {
            nodes.push(ts_node_child(current, i));
        }
    }
    
    return "";
}


} // namespace catchy::analysis
