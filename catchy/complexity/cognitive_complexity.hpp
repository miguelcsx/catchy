#ifndef CATCHY_COMPLEXITY_COGNITIVE_COMPLEXITY_HPP
#define CATCHY_COMPLEXITY_COGNITIVE_COMPLEXITY_HPP

#pragma once

#include <string>
#include <vector>
#include <tree_sitter/api.h>
#include <map>


namespace catchy::complexity {

struct ComplexityFactor {
    std::string description;
    size_t increment;
    size_t line_number;
};

struct ComplexityResult {
    size_t total_complexity{0};
    size_t nesting_level{0};
    std::vector<ComplexityFactor> factors;
    
    // Add map to track per-function complexity
    std::map<std::string, size_t> function_complexities;
};

class CognitiveComplexity {
public:
    ComplexityResult calculate(TSNode root_node, const std::string &source_code);

private:
    // Increment complexity based on different factors
    void increment_for_nesting(ComplexityResult& result, size_t increment, const std::string& reason, size_t line_number);
    void increment_for_structural(ComplexityResult& result, const std::string& reason, size_t line_number);
    void increment_for_fundamental(ComplexityResult& result, const std::string& reason, size_t line_number);
    void increment_for_hybrid(ComplexityResult& result, const std::string& reason, size_t line_number);

    // Analyze specific structures
    void analyze_control_flow(TSNode node, const std::string& source_code, ComplexityResult& result);
    void analyze_boolean_operators(TSNode node, ComplexityResult& result);
    void analyze_exceptions(TSNode node, ComplexityResult& result);
    void analyze_switch(TSNode node, ComplexityResult& result);
    void analyze_recursion(TSNode node, ComplexityResult& result);

    // Helper functions
    bool is_control_structure(TSNode node);
    bool is_boolean_operator(TSNode node);
    bool increases_nesting_level(TSNode node);
    TSNode find_function_name(TSNode declarator);
    std::string extract_node_text(TSNode node, const std::string &source_code);
};

} // namespace catchy::complexity

#endif // CATCHY_COMPLEXITY_COGNITIVE_COMPLEXITY_HPP
