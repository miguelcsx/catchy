#include "cpp_parser.hpp"
#include "utils/safe_conversions.hpp"
#include <spdlog/spdlog.h>
#include <stack>

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
        spdlog::debug("File content:\n{}", context.file_content);
        
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
        spdlog::debug("Root node type: {}", ts_node_type(root_node));
        
        collect_functions(root_node, context.file_content, functions);
        
        spdlog::debug("Found {} functions", functions.size());
        for (const auto& func : functions) {
            spdlog::debug("Function: {} (lines {}-{})", func.name, func.start_line, func.end_line);
            spdlog::debug("Function body:\n{}", func.body);
        }
        
        ts_tree_delete(tree);
    } catch (const std::exception& e) {
        spdlog::error("Error parsing functions in {}: {}", context.file_path, e.what());
    }
    
    return functions;
}

void CppParser::collect_functions(TSNode node, const std::string& source, std::vector<FunctionInfo>& functions) {
    if (ts_node_is_null(node)) {
        spdlog::debug("Skipping null node");
        return;
    }
    
    const char* type = ts_node_type(node);
    spdlog::debug("Processing node type: {}", type);
    
    try {
        // Check if this is a function definition
        if (strcmp(type, "function_definition") == 0) {
            spdlog::debug("Found function definition");
            FunctionInfo info;
            
            // Get function name
            TSNode declarator = ts_node_child_by_field_name(node, "declarator", strlen("declarator"));
            if (!ts_node_is_null(declarator)) {
                spdlog::debug("Found declarator node type: {}", ts_node_type(declarator));
                
                TSNode name_node = find_function_name(declarator);
                if (!ts_node_is_null(name_node)) {
                    info.name = extract_node_text(name_node, source);
                    spdlog::debug("Found function name: {}", info.name);
                } else {
                    spdlog::debug("Could not find function name in declarator");
                }
            } else {
                spdlog::warn("Could not find declarator node");
            }
            
            // Get function parameters
            TSNode parameter_list = ts_node_child_by_field_name(node, "parameters", strlen("parameters"));
            if (!ts_node_is_null(parameter_list)) {
                collect_parameters(parameter_list, source, info.parameters);
                spdlog::debug("Found {} parameters", info.parameters.size());
            } else {
                spdlog::debug("No parameters found");
            }
            
            // Get function body
            TSNode body = ts_node_child_by_field_name(node, "body", strlen("body"));
            if (!ts_node_is_null(body)) {
                info.body = extract_node_text(body, source);
                spdlog::debug("Found function body, length: {}", info.body.length());
                spdlog::debug("Function body:\n{}", info.body);
            } else {
                spdlog::debug("No function body found");
            }
            
            // Get line numbers
            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            info.start_line = utils::safe_cast<size_t>(start.row + 1);
            info.end_line = utils::safe_cast<size_t>(end.row + 1);
            
            spdlog::debug("Adding function {} (lines {}-{})", info.name, info.start_line, info.end_line);
            functions.push_back(std::move(info));
            return;  // Don't need to process children of function definition
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing node {}: {}", type, e.what());
    }
    
    // Recursively process children
    uint32_t child_count = ts_node_child_count(node);
    spdlog::debug("Node {} has {} children", type, child_count);
    
    for (uint32_t i = 0; i < child_count; i++) {
        collect_functions(ts_node_child(node, i), source, functions);
    }
}

std::vector<std::string> CppParser::get_extensions() const {
    return {"cpp", "cxx", "cc", "hpp", "hxx", "h"};
}

std::string CppParser::get_language_name() const {
    return "cpp";
}

TSNode CppParser::find_function_name(TSNode declarator) {
    std::stack<TSNode> nodes;
    nodes.push(declarator);
    
    spdlog::debug("Looking for function name in declarator of type: {}", 
                  ts_node_type(declarator));
    
    while (!nodes.empty()) {
        TSNode current = nodes.top();
        nodes.pop();
        
        const char* type = ts_node_type(current);
        spdlog::debug("Checking node type: {}", type);
        
        if (strcmp(type, "identifier") == 0) {
            spdlog::debug("Found identifier: {}", extract_node_text(current, ""));
            return current;
        }
        
        uint32_t child_count = ts_node_child_count(current);
        for (uint32_t i = 0; i < child_count; i++) {
            nodes.push(ts_node_child(current, i));
        }
    }
    
    spdlog::debug("No identifier found in declarator");
    return TSNode{};
}

void CppParser::collect_parameters(TSNode parameter_list, const std::string& source, std::vector<std::string>& parameters) {
    uint32_t child_count = ts_node_child_count(parameter_list);
    spdlog::debug("Parameter list has {} children", child_count);
    
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(parameter_list, i);
        const char* type = ts_node_type(child);
        spdlog::debug("Processing parameter node type: {}", type);
        
        if (strcmp(type, "parameter_declaration") == 0) {
            TSNode declarator = ts_node_child_by_field_name(child, "declarator", strlen("declarator"));
            if (!ts_node_is_null(declarator)) {
                TSNode name_node = find_function_name(declarator);
                if (!ts_node_is_null(name_node)) {
                    std::string param_name = extract_node_text(name_node, source);
                    spdlog::debug("Found parameter: {}", param_name);
                    parameters.push_back(std::move(param_name));
                }
            }
        }
    }
}

} // namespace catchy::parser::languages
