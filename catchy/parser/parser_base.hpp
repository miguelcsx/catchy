#ifndef CATCHY_PARSER_BASE_HPP
#define CATCHY_PARSER_BASE_HPP

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <tree_sitter/api.h>
#include <spdlog/spdlog.h>

namespace catchy::parser {

struct FunctionInfo {
    std::string name;
    size_t start_line;
    size_t end_line;
    std::string body;
    TSNode node;
    std::vector<std::string> parameters;
};

struct ParserContext {
    std::string file_content;
    std::string file_path;
};

class ParserBase {
public:
    // Non-copyable but movable
    ParserBase() : parser_(ts_parser_new(), ts_parser_delete) {}
    ParserBase(const ParserBase&) = delete;
    ParserBase& operator=(const ParserBase&) = delete;
    ParserBase(ParserBase&&) = default;
    ParserBase& operator=(ParserBase&&) = default;
    virtual ~ParserBase() = default;

    // Create a clone of this parser
    virtual std::unique_ptr<ParserBase> clone() const = 0;

    virtual bool initialize() = 0;
    virtual std::vector<FunctionInfo> parse_functions(const ParserContext &context) = 0;
    virtual std::vector<std::string> get_extensions() const = 0;
    virtual std::string get_language_name() const = 0;

protected:
    // Helper functions for tree-sitter operations
    std::string extract_node_text(const TSNode &node, const std::string &source_code);
    std::optional<std::string> get_function_name(const TSNode &node);

    std::unique_ptr<TSParser, void(*)(TSParser*)> parser_;
};

} // namespace catchy::parser

#endif // CATCHY_PARSER_BASE_HPP
