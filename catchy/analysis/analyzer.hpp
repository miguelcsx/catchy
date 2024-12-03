// analyzer.hpp
#ifndef CATCHY_ANALYSIS_ANALYZER_HPP
#define CATCHY_ANALYSIS_ANALYZER_HPP

#pragma once

#include "parser/parser_factory.hpp"
#include "complexity/cognitive_complexity.hpp"
#include <string>
#include <vector>
#include <memory>
#include <tree_sitter/api.h>

namespace catchy::analysis {

struct AnalysisResult {
    std::string file_path;
    std::string language;
    std::string function_name;
    size_t start_line;
    size_t end_line;
    size_t complexity;
    std::vector<complexity::ComplexityFactor> factors;

    // Serialize to TOML
    std::string to_toml() const;
};

class Analyzer {
public:
    Analyzer();
    ~Analyzer();

    // Analysis methods
    std::vector<AnalysisResult> analyze_file(const std::string &file_path);
    std::vector<AnalysisResult> analyze_directory(const std::string &directory_path, bool recursive = false);
    std::vector<AnalysisResult> analyze_git_repository(const std::string &repository_path);

    // Configuration
    void set_language(const std::string &language) { language_ = language; }
    void set_complexity_threshold(size_t threshold) { complexity_threshold_ = threshold; }
    void set_ignore_patterns(const std::vector<std::string> &patterns) { ignore_patterns_ = patterns; }

private:
    std::vector<AnalysisResult> analyze_content(const std::string &content, const std::string &file_path, const std::string &language);
    bool should_analyze_file(const std::string &file_path) const;
    std::string detect_language(const std::string &file_path) const;
    
    // Tree-sitter helper functions
    TSNode find_function_node(TSNode node, const std::string& function_name, const std::string& source);
    std::string extract_function_name(TSNode node, const std::string& source);

    std::string language_;
    size_t complexity_threshold_ {0};
    std::vector<std::string> ignore_patterns_;
    std::unique_ptr<complexity::CognitiveComplexity> complexity_calculator_;
};

} // namespace catchy::analysis

#endif // CATCHY_ANALYSIS_ANALYZER_HPP
