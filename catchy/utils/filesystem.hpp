#ifndef CATCHY_UTILS_FILESYSTEM_HPP
#define CATCHY_UTILS_FILESYSTEM_HPP

#pragma once

#include <string>
#include <vector>

namespace catchy::utils {

// File reading operations
std::string read_file_content(const std::string &file_path);

//  Directory operations
std::vector<std::string> list_files(const std::string &directory_path, bool recursive = false);


// Pattern matching
bool matches_pattern(const std::string &text, const std::string &pattern);

// Path operations
std::string normalize_path(const std::string &path);
std::string get_relative_path(const std::string &base, const std::string &path);

} // namespace catchy::utils

#endif // CATCHY_UTILS_FILESYSTEM_HPP
