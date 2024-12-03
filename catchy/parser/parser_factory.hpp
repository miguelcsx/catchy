#ifndef CATCHY_PARSER_FACTORY_HPP
#define CATCHY_PARSER_FACTORY_HPP

#pragma once

#include "parser_base.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace catchy::parser {

class ParserFactory {
public:
    static ParserFactory& instance() {
        static ParserFactory factory;
        return factory;
    }

    // Register a new parser type
    template<typename T>
    void register_parser() {
        auto parser = std::make_unique<T>();
        if (parser->initialize()) {
            register_parser(std::move(parser));
        }
    }

    void register_parser(std::unique_ptr<ParserBase> parser);

    // Get parser by language name
    std::unique_ptr<ParserBase> create_parser(const std::string &language);

    // Get parser by file extension
    std::unique_ptr<ParserBase> create_parser_for_file(const std::string &file_path);

    // Get supported languages
    std::vector<std::string> get_supported_languages() const;

    // Get supported file extensions
    std::vector<std::string> get_supported_extensions() const;

private:
    ParserFactory() = default;
    ParserFactory(const ParserFactory&) = delete;
    ParserFactory& operator=(const ParserFactory&) = delete;

    std::unordered_map<std::string, std::unique_ptr<ParserBase>> parsers_;
    std::unordered_map<std::string, std::string> extensions_;
};

} // namespace catchy::parser

#endif // CATCHY_PARSER_FACTORY_HPP
