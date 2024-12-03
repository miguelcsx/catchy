#include "python_parser.hpp"
#include "utils/safe_conversions.hpp"
#include <spdlog/spdlog.h>
#include <stack>

namespace catchy::parser::languages {


std::unique_ptr<ParserBase> PythonParser::clone() const {
    return std::make_unique<PythonParser>();
}

bool PythonParser::initialize() {
    if (!parser_) {
        spdlog::error("Parser not initialized");
        return false;
    }
    
    spdlog::debug("Setting up Python parser with tree-sitter");
    ts_parser_set_language(parser_.get(), tree_sitter_python());
    return true;
}

std::vector<FunctionInfo> PythonParser::parse_functions(const ParserContext& context) {
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


std::vector<std::string> PythonParser::get_extensions() const {
    return {"py"};
}

std::string PythonParser::get_language_name() const {
    return "python";
}

TSNode PythonParser::find_function_name(TSNode declarator) {
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

void PythonParser::collect_functions(TSNode node, const std::string& source, std::vector<FunctionInfo>& functions) {
    const char* type = ts_node_type(node);
    spdlog::debug("Processing Python node type: {}", type);
    
    try {
        if (strcmp(type, "function_definition") == 0 || 
            strcmp(type, "decorated_definition") == 0) {
            
            TSNode func_node = node;
            if (strcmp(type, "decorated_definition") == 0) {
                func_node = ts_node_child_by_field_name(node, "definition", strlen("definition"));
                if (ts_node_is_null(func_node)) {
                    spdlog::error("Could not find definition in decorated_definition");
                    return;
                }
            }
            
            FunctionInfo info;
            info.node = func_node;  // Store the actual function node
            
            // Get function name
            TSNode name_node = ts_node_child_by_field_name(func_node, "name", strlen("name"));
            if (!ts_node_is_null(name_node)) {
                info.name = extract_node_text(name_node, source);
                spdlog::debug("Found Python function: {}", info.name);
                
                // Handle nested functions
                TSNode parent = ts_node_parent(func_node);
                while (!ts_node_is_null(parent)) {
                    const char* parent_type = ts_node_type(parent);
                    if (strcmp(parent_type, "function_definition") == 0) {
                        TSNode parent_name = ts_node_child_by_field_name(parent, "name", strlen("name"));
                        if (!ts_node_is_null(parent_name)) {
                            std::string parent_name_str = extract_node_text(parent_name, source);
                            info.name = parent_name_str + "." + info.name;
                            spdlog::debug("Updated nested function name to: {}", info.name);
                        }
                    }
                    parent = ts_node_parent(parent);
                }
            }

            // Get function body
            TSNode body = ts_node_child_by_field_name(func_node, "body", strlen("body"));
            if (!ts_node_is_null(body)) {
                info.body = extract_node_text(body, source);
            }
            
            // Get line numbers
            TSPoint start = ts_node_start_point(func_node);
            TSPoint end = ts_node_end_point(func_node);
            info.start_line = start.row + 1;
            info.end_line = end.row + 1;
            
            spdlog::debug("Adding Python function {} (lines {}-{})", 
                         info.name, info.start_line, info.end_line);
            functions.push_back(std::move(info));
        }

        // Process children
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            collect_functions(ts_node_child(node, i), source, functions);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing Python node {}: {}", type, e.what());
    }
}


void PythonParser::collect_parameters(TSNode parameter_list, const std::string& source, std::vector<std::string>& parameters) {
    uint32_t child_count = ts_node_child_count(parameter_list);
    spdlog::debug("Found {} parameter nodes", child_count);
    
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode param = ts_node_child(parameter_list, i);
        const char* param_type = ts_node_type(param);
        
        if (strcmp(param_type, "identifier") == 0) {
            parameters.push_back(extract_node_text(param, source));
        }
        else if (strcmp(param_type, "typed_parameter") == 0 || 
                 strcmp(param_type, "default_parameter") == 0) {
            TSNode name_node = ts_node_child_by_field_name(param, "name", strlen("name"));
            if (!ts_node_is_null(name_node)) {
                parameters.push_back(extract_node_text(name_node, source));
            }
        }
    }
}

} // namespace catchy::parser::languages
