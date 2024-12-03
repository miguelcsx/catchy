#include "parser_factory.hpp"
#include <filesystem>
#include <spdlog/spdlog.h>

namespace catchy::parser {

void ParserFactory::register_parser(std::unique_ptr<ParserBase> parser) {
    if (!parser) {
        spdlog::error("Attempting to register null parser");
        return;
    }

    auto language_name = parser->get_language_name();
    auto extensions = parser->get_extensions();

    // Store the parser
    parsers_[language_name] = std::move(parser);

    // Register extensions
    for (const auto& ext : extensions) {
        extensions_[ext] = language_name;
    }
}

std::unique_ptr<ParserBase> ParserFactory::create_parser(const std::string &language) {
    auto it = parsers_.find(language);
    if (it != parsers_.end()) {
        return it->second->clone();
    }
    return nullptr;
}

std::unique_ptr<ParserBase> ParserFactory::create_parser_for_file(const std::string& filename) {
    std::string ext = std::filesystem::path(filename).extension().string();
    if (ext.empty()) return nullptr;
    
    // Remove the dot from extension
    if (ext[0] == '.') {
        ext = ext.substr(1);
    }

    auto it = extensions_.find(ext);
    if (it != extensions_.end()) {
        return create_parser(it->second);
    }
    return nullptr;
}

std::vector<std::string> ParserFactory::get_supported_languages() const {
    std::vector<std::string> languages;
    languages.reserve(parsers_.size());
    for (const auto& [lang, _] : parsers_) {
        languages.push_back(lang);
    }
    return languages;
}

std::vector<std::string> ParserFactory::get_supported_extensions() const {
    std::vector<std::string> extensions;
    extensions.reserve(extensions_.size());
    for (const auto& [ext, _] : extensions_) {
        extensions.push_back(ext);
    }
    return extensions;
}

} // namespace catchy::parser
