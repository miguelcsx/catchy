#include "cognitive_complexity.hpp"
#include "utils/safe_conversions.hpp"
#include <string>
#include <cstring>
#include <stack>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace catchy::complexity {

ComplexityResult CognitiveComplexity::calculate(TSNode root_node, const std::string &source_code) {
    ComplexityResult total_result;

    if (ts_node_is_null(root_node)) {
        spdlog::error("Received null root node");
        return total_result;
    }

    std::stack<TSNode> nodes;
    nodes.push(root_node);

    while (!nodes.empty()) {
        TSNode current = nodes.top();
        nodes.pop();

        if (!ts_node_is_null(current)) {
            const char* curr_type = ts_node_type(current);

            if (strcmp(curr_type, "function_definition") == 0 || strcmp(curr_type, "function_declaration") == 0) {
                // Get function name
                TSNode declarator = ts_node_child_by_field_name(current, "declarator", strlen("declarator"));
                std::string func_name = "unknown";
                if (!ts_node_is_null(declarator)) {
                    TSNode name_node = find_function_name(declarator);
                    if (!ts_node_is_null(name_node)) {
                        func_name = extract_node_text(name_node, source_code);
                    }
                }

                // Analyze function body
                TSNode body = ts_node_child_by_field_name(current, "body", strlen("body"));
                if (!ts_node_is_null(body)) {
                    ComplexityResult func_result;
                    analyze_control_flow(body, source_code, func_result);

                    // Store function's complexity
                    total_result.function_complexities[func_name] = func_result.total_complexity;

                    // Correctly add function complexity to the total
                    total_result.total_complexity += func_result.total_complexity;

                    // Append factors from this function to the total
                    total_result.factors.insert(
                        total_result.factors.end(),
                        func_result.factors.begin(),
                        func_result.factors.end()
                    );

                    spdlog::debug("Function '{}' complexity: {}", func_name, func_result.total_complexity);
                }
            }

            // Add all children to the stack
            uint32_t child_count = ts_node_child_count(current);
            for (uint32_t i = child_count; i > 0; --i) {
                nodes.push(ts_node_child(current, i - 1));
            }
        }
    }

    spdlog::info("Total cognitive complexity: {}", total_result.total_complexity);
    return total_result;
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
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    try {
        TSPoint start_point = ts_node_start_point(node);
        size_t line_number = start_point.row + 1;

        // Check if current node increases nesting level
        bool increases_nesting = increases_nesting_level(node);

        // Analyze current node
        if (is_control_structure(node)) {
            bool is_else_if = false;
            if (strcmp(type, "if_statement") == 0) {
                TSNode parent = ts_node_parent(node);
                if (!ts_node_is_null(parent) && strcmp(ts_node_type(parent), "else_clause") == 0) {
                    is_else_if = true;
                    increment_for_hybrid(result, "else-if statement", line_number);
                }
            }

            if (!is_else_if) {
                // Base increment for control structure
                increment_for_structural(result, std::string("Control structure: ") + type, line_number);

                // Add nesting increment based on current nesting level
                if (increases_nesting && result.nesting_level > 0) {
                    increment_for_nesting(result, result.nesting_level,
                        std::string("Nested ") + type, line_number);
                }
            }
        }

        // Increase nesting level if needed
        if (increases_nesting) {
            result.nesting_level++;
        }

        // Process children
        uint32_t child_count = ts_node_child_count(node);
        for (uint32_t i = 0; i < child_count; i++) {
            analyze_control_flow(ts_node_child(node, i), source_code, result);
        }

        // Restore nesting level
        if (increases_nesting) {
            result.nesting_level--;
        }

    } catch (const std::exception& e) {
        spdlog::error("Error analyzing node {}: {}", type, e.what());
    }
}

bool CognitiveComplexity::is_control_structure(TSNode node) {
    const char* type = ts_node_type(node);
    static const std::unordered_set<std::string> control_structures = {
        "if_statement",
        "for_statement",
        "for_range_loop",  // For range-based for loops
        "while_statement",
        "do_statement",
        "catch_clause",
        "switch_statement",
        "case_statement"
    };
    
    return control_structures.count(type) > 0;
}

bool CognitiveComplexity::is_boolean_operator(TSNode node) {
    const char* type = ts_node_type(node);
    return strcmp(type, "binary_expression") == 0;
}

bool CognitiveComplexity::increases_nesting_level(TSNode node) {
    const char* type = ts_node_type(node);
    static const std::unordered_set<std::string> nesting_structures = {
        "if_statement",
        "for_statement",
        "for_range_loop",
        "while_statement",
        "do_statement",
        "catch_clause",
        "switch_statement"
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
