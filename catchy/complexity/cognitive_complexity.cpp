#include "cognitive_complexity.hpp"
#include "utils/safe_conversions.hpp"
#include <string>
#include <cstring>
#include <stack>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace catchy::complexity {

ComplexityResult CognitiveComplexity::calculate(TSNode root_node, const std::string &source_code) {
    ComplexityResult result;

    if (ts_node_is_null(root_node)) {
        spdlog::error("Received null root node in calculate");
        return result;
    }

    // Find the actual function body
    TSNode body_node = root_node;
    bool is_function = false;

    try {
        const char* node_type = ts_node_type(root_node);
        spdlog::debug("Root node type in calculate: {}", node_type ? node_type : "null");

        // Get the actual body for analysis
        if (node_type && (strcmp(node_type, "function_definition") == 0 || 
                         strcmp(node_type, "method_definition") == 0)) {
            is_function = true;
            body_node = ts_node_child_by_field_name(root_node, "body", strlen("body"));
            if (ts_node_is_null(body_node)) {
                spdlog::debug("Function body is null");
                return result;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in calculate: {}", e.what());
        return result;
    }

    result.nesting_level = 0;
    analyze_control_flow(body_node, source_code, result);
    return result;
}


TSNode CognitiveComplexity::find_function_name(TSNode declarator) {
    // Look for function_declarator or identifier

    if (strcmp(ts_node_type(declarator), "function_declarator") == 0) {
        return ts_node_child_by_field_name(declarator, "declarator", strlen("declarator"));
    }

    return declarator;
}

std::string CognitiveComplexity::extract_node_text(TSNode node, const std::string &source_code) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    return source_code.substr(start, end - start);
}


void CognitiveComplexity::analyze_control_flow(TSNode node, const std::string& source_code, ComplexityResult& result) {
    if (ts_node_is_null(node)) {
        return;
    }

    try {
        const char* node_type = nullptr;
        try {
            node_type = ts_node_type(node);
        } catch (...) {
            spdlog::error("Failed to get node type");
            return;
        }

        if (!node_type) {
            spdlog::error("Node type is null");
            return;
        }

        TSPoint start_point = ts_node_start_point(node);
        size_t line_number = start_point.row + 1;

        // Skip nested function definitions when calculating complexity
        if (strcmp(node_type, "function_definition") == 0) {
            TSNode parent = ts_node_parent(node);
            // Only process the function body if this is not a nested function
            bool is_nested = false;
            while (!ts_node_is_null(parent)) {
                const char* parent_type = ts_node_type(parent);
                if (strcmp(parent_type, "function_definition") == 0) {
                    is_nested = true;
                    break;
                }
                parent = ts_node_parent(parent);
            }
            
            if (!is_nested) {
                // Process the function body for non-nested functions
                TSNode body = ts_node_child_by_field_name(node, "body", strlen("body"));
                if (!ts_node_is_null(body)) {
                    analyze_control_flow(body, source_code, result);
                }
            }
            return;
        }

        // Check for control structures
        if (is_control_structure(node_type)) {
            bool is_else_if = false;

            // Handle else-if chains
            if (strcmp(node_type, "if_statement") == 0) {
                TSNode parent = ts_node_parent(node);
                if (!ts_node_is_null(parent)) {
                    const char* parent_type = ts_node_type(parent);
                    if (parent_type && (strcmp(parent_type, "else_clause") == 0 || 
                                      strcmp(parent_type, "elif_clause") == 0)) {
                        is_else_if = true;
                        increment_for_hybrid(result, "else-if chain", line_number);
                    }
                }
            }

            if (!is_else_if) {
                // Base increment for control structure
                increment_for_structural(result, std::string(node_type), line_number);

                // Add nesting increment if needed
                if (increases_nesting_level(node_type) && result.nesting_level > 0) {
                    increment_for_nesting(result, result.nesting_level,
                        std::string("Nested ") + node_type, line_number);
                }
            }
        }

        // Handle nesting
        bool increases_nesting = increases_nesting_level(node_type);
        if (increases_nesting) {
            result.nesting_level++;
        }

        // Process children
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            if (!ts_node_is_null(child)) {
                analyze_control_flow(child, source_code, result);
            }
        }

        if (increases_nesting) {
            result.nesting_level--;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in analyze_control_flow: {}", e.what());
    }
}


bool CognitiveComplexity::is_control_structure(const char* type) {
    static const std::unordered_set<std::string> control_structures = {
        "if_statement",
        "for_statement",
        "while_statement",
        "do_statement",
        "catch_clause",
        "case_statement",
        "for_range_loop",
        "elif_clause",
        "else_clause"
    };
    
    return control_structures.count(type) > 0;
}

bool CognitiveComplexity::is_boolean_operator(TSNode node) {
    const char* type = ts_node_type(node);
    return strcmp(type, "binary_expression") == 0;
}

bool CognitiveComplexity::increases_nesting_level(const char* type) {
    static const std::unordered_set<std::string> nesting_structures = {
        "if_statement",
        "for_statement",
        "while_statement",
        "do_statement",
        "catch_clause",
        "for_range_loop"
    };
    
    return nesting_structures.count(type) > 0;
}

void CognitiveComplexity::analyze_boolean_operators(TSNode node, ComplexityResult& result) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);
    if (strcmp(type, "binary_expression") == 0) {
        TSNode operator_node = ts_node_child(node, 1);
        if (!ts_node_is_null(operator_node)) {
            const char* operator_type = ts_node_type(operator_node);
            TSPoint start_point = ts_node_start_point(node);
            size_t line_number = start_point.row + 1;
            
            if (strcmp(operator_type, "&&") == 0 || strcmp(operator_type, "||") == 0) {
                spdlog::debug("Found boolean operator: {} at line {}", operator_type, line_number);
                increment_for_fundamental(result, 
                    std::string("Boolean operator: ") + operator_type,
                    line_number);
            }
        }
    }
}

void CognitiveComplexity::increment_for_nesting(ComplexityResult& result, 
                                              size_t nesting_level, 
                                              const std::string& reason,
                                              size_t line_number) {
    result.total_complexity += nesting_level;
    result.factors.push_back({
        reason,
        nesting_level,
        line_number
    });
    spdlog::debug("Added nesting complexity: +{} for {} at line {}", 
                 nesting_level, reason, line_number);
}

void CognitiveComplexity::increment_for_structural(ComplexityResult& result, 
                                                 const std::string& reason,
                                                 size_t line_number) {
    result.total_complexity += 1;
    result.factors.push_back({
        reason,
        1,
        line_number
    });
    spdlog::debug("Added structural complexity: +1 for {} at line {}", 
                 reason, line_number);
}

void CognitiveComplexity::increment_for_fundamental(ComplexityResult& result, 
                                                  const std::string& reason,
                                                  size_t line_number) {
    result.total_complexity += 1;
    result.factors.push_back({
        reason,
        1,
        line_number
    });
    spdlog::debug("Added fundamental complexity: +1 for {} at line {}", 
                 reason, line_number);
}

void CognitiveComplexity::increment_for_hybrid(ComplexityResult& result, 
                                             const std::string& reason,
                                             size_t line_number) {
    result.total_complexity += 1;
    result.factors.push_back({
        reason,
        1,
        line_number
    });
    spdlog::debug("Added hybrid complexity: +1 for {} at line {}", 
                 reason, line_number);
}

} // namespace catchy::complexity
