#include "cpp_parser.hpp"
#include "utils/safe_conversions.hpp"
#include <spdlog/spdlog.h>
#include <stack>
#include <unordered_set>

namespace catchy::parser::languages {

std::unique_ptr<ParserBase> CppParser::clone() const {
    return std::make_unique<CppParser>();
}

bool CppParser::initialize() {
    if (!parser_) {
        spdlog::error("Parser not initialized");
        return false;
    }
    
    spdlog::debug("Setting up C++ parser with tree-sitter");
    ts_parser_set_language(parser_.get(), tree_sitter_cpp());
    return true;
}

std::vector<FunctionInfo> CppParser::parse_functions(const ParserContext& context) {
    std::vector<FunctionInfo> functions;
    
    if (context.file_content.empty()) {
        spdlog::warn("Empty file content");
        return functions;
    }

    try {
        spdlog::debug("Parsing file: {}", context.file_path);
        
        TSTree* tree = ts_parser_parse_string(
            parser_.get(),
            nullptr,
            context.file_content.c_str(),
            utils::safe_string_length(context.file_content)
        );
        
        if (!tree) {
            spdlog::error("Failed to parse file: {}", context.file_path);
            return functions;
        }

        TSNode root_node = ts_tree_root_node(tree);
        collect_functions(root_node, context.file_content, functions, "");
        
        ts_tree_delete(tree);
    } catch (const std::exception& e) {
        spdlog::error("Error parsing functions in {}: {}", context.file_path, e.what());
    }
    
    return functions;
}

void CppParser::collect_functions(TSNode node, const std::string& source, 
                                std::vector<FunctionInfo>& functions,
                                const std::string& class_scope) {
    if (ts_node_is_null(node)) {
        return;
    }
    
    const char* type = ts_node_type(node);
    
    try {
        if (strcmp(type, "function_definition") == 0) {
            FunctionInfo info;
            
            // Store the entire function node
            info.node = node;
            
            // Get function name
            TSNode declarator = ts_node_child_by_field_name(node, "declarator", strlen("declarator"));
            if (!ts_node_is_null(declarator)) {
                TSNode name_node = find_function_name(declarator);
                if (!ts_node_is_null(name_node)) {
                    info.name = extract_node_text(name_node, source);
                    if (!class_scope.empty()) {
                        info.name = class_scope + "::" + info.name;
                    }
                    
                    // Add debug logging
                    spdlog::debug("Found C++ function: {} at node type {}", info.name, type);
                    
                    // Get function body
                    TSNode body = ts_node_child_by_field_name(node, "body", strlen("body"));
                    if (!ts_node_is_null(body)) {
                        info.body = extract_node_text(body, source);
                    }
                    
                    // Get line numbers
                    TSPoint start = ts_node_start_point(node);
                    TSPoint end = ts_node_end_point(node);
                    info.start_line = utils::safe_cast<size_t>(start.row + 1);
                    info.end_line = utils::safe_cast<size_t>(end.row + 1);
                    
                    spdlog::debug("Adding C++ function {} (lines {}-{})", 
                                 info.name, info.start_line, info.end_line);
                    functions.push_back(std::move(info));
                    return;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing C++ node {}: {}", type, e.what());
    }
    
    // Process children
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        collect_functions(ts_node_child(node, i), source, functions, class_scope);
    }
}

TSNode CppParser::find_function_name(TSNode declarator) {
    if (ts_node_is_null(declarator)) {
        return TSNode{};
    }

    std::stack<TSNode> nodes;
    nodes.push(declarator);
    
    while (!nodes.empty()) {
        TSNode current = nodes.top();
        nodes.pop();
        
        const char* type = ts_node_type(current);
        spdlog::debug("Finding function name in node type: {}", type);
        
        // Handle function declarator specifically
        if (strcmp(type, "function_declarator") == 0) {
            TSNode decl = ts_node_child_by_field_name(current, "declarator", strlen("declarator"));
            if (!ts_node_is_null(decl)) {
                TSNode name = find_function_name(decl);
                if (!ts_node_is_null(name)) {
                    return name;
                }
            }
            continue;
        }
        
        // Direct identifier match
        if (strcmp(type, "identifier") == 0) {
            return current;
        }
        
        // Special cases
        if (strcmp(type, "qualified_identifier") == 0 || 
            strcmp(type, "scoped_identifier") == 0) {
            TSNode name = ts_node_child_by_field_name(current, "name", strlen("name"));
            if (!ts_node_is_null(name)) {
                return name;
            }
        }
        
        // Add all children to stack
        uint32_t child_count = ts_node_child_count(current);
        for (uint32_t i = child_count; i > 0; --i) {
            nodes.push(ts_node_child(current, i - 1));
        }
    }
    
    return TSNode{};
}

void CppParser::collect_parameters(TSNode declarator, const std::string& source, std::vector<std::string>& parameters) {
    // Find the parameter list node
    std::stack<TSNode> nodes;
    nodes.push(declarator);
    
    while (!nodes.empty()) {
        TSNode current = nodes.top();
        nodes.pop();
        
        const char* type = ts_node_type(current);
        
        if (strcmp(type, "parameter_list") == 0) {
            uint32_t param_count = ts_node_child_count(current);
            for (uint32_t i = 0; i < param_count; i++) {
                TSNode param = ts_node_child(current, i);
                if (strcmp(ts_node_type(param), "parameter_declaration") == 0) {
                    TSNode param_declarator = ts_node_child_by_field_name(param, "declarator", strlen("declarator"));
                    if (!ts_node_is_null(param_declarator)) {
                        TSNode param_name = find_function_name(param_declarator);
                        if (!ts_node_is_null(param_name)) {
                            parameters.push_back(extract_node_text(param_name, source));
                        }
                    }
                }
            }
            return;
        }
        
        uint32_t child_count = ts_node_child_count(current);
        for (uint32_t i = 0; i < child_count; i++) {
            nodes.push(ts_node_child(current, i));
        }
    }
}

std::vector<std::string> CppParser::get_extensions() const {
    return {"cpp", "cxx", "cc", "hpp", "hxx", "h"};
}

std::string CppParser::get_language_name() const {
    return "cpp";
}

} // namespace catchy::parser::languages
