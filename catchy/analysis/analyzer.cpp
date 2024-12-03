#include "analyzer.hpp"
#include "parser/parser_factory.hpp"
#include "parser/languages/cpp_parser.hpp"
#include "parser/languages/python_parser.hpp"
#include "utils/safe_conversions.hpp"
#include "utils/filesystem.hpp"
#include "utils/git.hpp"
#include <spdlog/spdlog.h>
#include <llvm/Support/raw_ostream.h>
#include <stack>

namespace catchy::analysis {

Analyzer::Analyzer() 
    : complexity_calculator_(std::make_unique<complexity::CognitiveComplexity>()),
      parser_(ts_parser_new(), ts_parser_delete)
{
    if (!parser_) {
        throw std::runtime_error("Failed to create tree-sitter parser");
    }

    try {
        // Register parsers
        auto& factory = parser::ParserFactory::instance();
        
        // Register C++ parser
        factory.register_parser<parser::languages::CppParser>();
        spdlog::debug("Registered C++ parser");

        // Register Python parser
        factory.register_parser<parser::languages::PythonParser>();
        spdlog::debug("Registered Python parser");

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize parsers: {}", e.what());
        throw;
    }
}

std::vector<AnalysisResult> Analyzer::analyze_file(const std::string& file_path) {
    try {
        spdlog::info("Analyzing file: {}", file_path);
        
        // Read file content
        std::string content = utils::read_file_content(file_path);
        if (content.empty()) {
            spdlog::error("Empty file content for: {}", file_path);
            return {};
        }
        
        // Detect language
        std::string lang = language_.empty() ? detect_language(file_path) : language_;
        if (lang.empty()) {
            spdlog::error("Could not detect language for file: {}", file_path);
            return {};
        }
        spdlog::info("Detected language: {}", lang);
        
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
    auto& factory = parser::ParserFactory::instance();
    auto parser = factory.create_parser_for_file(file_path);
    
    if (parser) {
        auto lang = parser->get_language_name();
        spdlog::debug("Language detected: {} for file: {}", lang, file_path);
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
    
    try {
        // Set up parser for the correct language
        if (language == "cpp") {
            ts_parser_set_language(parser_.get(), tree_sitter_cpp());
        } else if (language == "python") {
            ts_parser_set_language(parser_.get(), tree_sitter_python());
        } else {
            spdlog::error("Unsupported language: {}", language);
            return results;
        }

        // Parse the entire file
        tree_.reset(ts_parser_parse_string(
            parser_.get(),
            nullptr,
            content.c_str(),
            static_cast<uint32_t>(content.length())
        ));

        if (!tree_) {
            spdlog::error("Failed to parse content");
            return results;
        }

        // Get functions
        auto parser = parser::ParserFactory::instance().create_parser(language);
        if (!parser || !parser->initialize()) {
            spdlog::error("Failed to initialize parser");
            return results;
        }

        parser::ParserContext context{content, file_path};
        auto functions = parser->parse_functions(context);
        
        spdlog::debug("Found {} functions to analyze", functions.size());
        
        // Analyze each function
        TSNode root_node = ts_tree_root_node(tree_.get());
        
        for (const auto& func : functions) {
            if (func.name.empty()) {
                continue;
            }

            AnalysisResult result;
            result.file_path = file_path;
            result.language = language;
            result.function_name = func.name;
            result.start_line = func.start_line;
            result.end_line = func.end_line;

            // Find function node in the parsed tree
            TSNode function_node = find_function_node(root_node, func.name, content);
            
            if (!ts_node_is_null(function_node)) {
                auto complexity_result = complexity_calculator_->calculate(function_node, content);
                result.complexity = complexity_result.total_complexity;
                result.factors = std::move(complexity_result.factors);
                
                if (result.complexity >= complexity_threshold_) {
                    results.push_back(std::move(result));
                }
            } else {
                spdlog::debug("Could not find node for function: {}", func.name);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in analyze_content: {}", e.what());
    }
    
    return results;
}

TSNode Analyzer::find_function_node(TSNode node, const std::string& function_name, const std::string& source) {
    if (ts_node_is_null(node)) {
        return node;
    }

    const char* type = ts_node_type(node);
    
    // Handle Python functions (including decorated and nested ones)
    if (strcmp(type, "function_definition") == 0 || 
        strcmp(type, "decorated_definition") == 0) {
        
        TSNode actual_func_node = node;
        if (strcmp(type, "decorated_definition") == 0) {
            actual_func_node = ts_node_child_by_field_name(node, "definition", strlen("definition"));
            if (ts_node_is_null(actual_func_node)) {
                return TSNode{};
            }
        }

        TSNode name_node = ts_node_child_by_field_name(actual_func_node, "name", strlen("name"));
        if (!ts_node_is_null(name_node)) {
            std::string current_name = extract_function_name(name_node, source);
            
            // Handle nested functions by building the full name
            std::string full_name = current_name;
            TSNode parent = ts_node_parent(actual_func_node);
            
            while (!ts_node_is_null(parent)) {
                if (strcmp(ts_node_type(parent), "function_definition") == 0) {
                    TSNode parent_name = ts_node_child_by_field_name(parent, "name", strlen("name"));
                    if (!ts_node_is_null(parent_name)) {
                        std::string parent_name_str = extract_function_name(parent_name, source);
                        full_name = parent_name_str + "." + full_name;
                    }
                }
                parent = ts_node_parent(parent);
            }
            
            if (full_name == function_name) {
                return actual_func_node;
            }
        }
    }
    
    // Handle C++ functions
    if (strcmp(type, "function_definition") == 0) {
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", strlen("declarator"));
        if (!ts_node_is_null(declarator)) {
            auto cpp_parser = std::make_unique<parser::languages::CppParser>();
            TSNode name_node = cpp_parser->find_function_name(declarator);
            
            if (!ts_node_is_null(name_node)) {
                std::string current_name = extract_function_name(name_node, source);
                if (current_name == function_name) {
                    return node;
                }
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
