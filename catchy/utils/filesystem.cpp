#include "filesystem.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>

namespace catchy::utils {

namespace fs = std::filesystem;

std::string read_file_content(const std::string &file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> list_files(const std::string& dir_path, bool recursive) {
    std::vector<std::string> files;
    try {
        if (recursive) {
            for (const auto &entry : fs::recursive_directory_iterator(dir_path)) {
                if (fs::is_regular_file(entry.path())) {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto &entry : fs::directory_iterator(dir_path)) {
                if (fs::is_regular_file(entry.path())) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const fs::filesystem_error &e) {
        throw std::runtime_error("Failed to list files in directory: " + dir_path);
    }

    return files;
}

bool matches_pattern(const std::string &text, const std::string &pattern) {
    try {
        std::regex regex(pattern);
        return std::regex_match(text, regex);
    } catch (const std::regex_error &e) {
        // If pattern is invalid, treat it as a simple string match
        return text.find(pattern) != std::string::npos;
    }
}

std::string normalize_path(const std::string &path) {
    return fs::path(path).lexically_normal().string();
}

std::string get_relative_path(const std::string &base, const std::string &path) {
    return fs::relative(path, base).string();
}

} // namespace catchy::utils
