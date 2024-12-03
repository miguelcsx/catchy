#ifndef CATCHY_PARSER_PYTHON_PARSER_HPP
#define CATCHY_PARSER_PYTHON_PARSER_HPP

#pragma once

#include "parser/parser_base.hpp"
#include <string>
#include <vector>
#include <memory>

extern "C" {
    const TSLanguage* tree_sitter_python();
}

namespace catchy::parser::languages {

struct PythonCodeBlock {
    std::string content;
    bool is_function;
    std::string function_name;
    std::vector<std::string> parameters;
};

class PythonParser : public ParserBase {
public:
    PythonParser() = default;
    
    PythonParser(const PythonParser&) = delete;
    PythonParser& operator=(const PythonParser&) = delete;
    PythonParser(PythonParser&&) = default;
    PythonParser& operator=(PythonParser&&) = default;

    ~PythonParser() override = default;

    std::unique_ptr<ParserBase> clone() const override;
    bool initialize() override;
    std::vector<FunctionInfo> parse_functions(const ParserContext& context) override;
    std::vector<std::string> get_extensions() const override;
    std::string get_language_name() const override;

private:
    void collect_code_blocks(TSNode node, const std::string& source, std::vector<PythonCodeBlock>& blocks);
    void collect_functions(TSNode node, const std::string& source, std::vector<FunctionInfo>& functions);
    TSNode find_function_name(TSNode declarator);
    void collect_parameters(TSNode parameter_list, const std::string& source, std::vector<std::string>& parameters);
    void handle_top_level_code(TSNode node, const std::string& source, std::vector<FunctionInfo>& functions);
};

} // namespace catchy::parser::languages

#endif // CATCHY_PARSER_PYTHON_PARSER_HPP
