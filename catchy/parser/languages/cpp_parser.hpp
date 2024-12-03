#ifndef CATCHY_PARSER_CPP_PARSER_HPP
#define CATCHY_PARSER_CPP_PARSER_HPP

#pragma once

#include "../parser_base.hpp"
#include <string>
#include <vector>
#include <memory>

// Forward declare the tree-sitter function
extern "C" {
    const TSLanguage* tree_sitter_cpp();
}

namespace catchy::parser::languages {

class CppParser : public ParserBase {
public:
    CppParser() = default;
    
    // Implement non-copyable but movable semantics
    CppParser(const CppParser&) = delete;
    CppParser& operator=(const CppParser&) = delete;
    CppParser(CppParser&&) = default;
    CppParser& operator=(CppParser&&) = default;

    // Virtual destructor
    ~CppParser() override = default;

    // Create a fresh instance instead of copying
    std::unique_ptr<ParserBase> clone() const override;
    bool initialize() override;
    std::vector<FunctionInfo> parse_functions(const ParserContext& context) override;
    std::vector<std::string> get_extensions() const override;
    std::string get_language_name() const override;

private:
    void collect_functions(TSNode node, const std::string& source, std::vector<FunctionInfo>& functions);
    TSNode find_function_name(TSNode declarator);
    void collect_parameters(TSNode parameter_list, const std::string& source, std::vector<std::string>& parameters);
};

} // namespace catchy::parser::languages

#endif // CATCHY_PARSER_CPP_PARSER_HPP
