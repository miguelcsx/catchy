#ifndef CATCHY_UTILS_GIT_HPP
#define CATCHY_UTILS_GIT_HPP

#pragma once

#include <string>
#include <vector>

namespace catchy::utils {

std::vector<std::string> list_git_files(const std::string &repository_path);
bool is_git_repo(const std::string &path);
std::string get_git_root(const std::string &path);

// Git status operations
bool is_file_tracked(const std::string &repo_path, const std::string &file_path);
bool has_uncommitted_changes(const std::string &repo_path, const std::string &file_path);


} // namespace catchy::utils

#endif // CATCHY_UTILS_GIT_HPP
