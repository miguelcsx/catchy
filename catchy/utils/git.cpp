#include "git.hpp"
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <array>
#include <sstream>
#include <filesystem>

namespace catchy::utils {
namespace {
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

} // namespace


bool is_git_repo(const std::string& path) {
    std::filesystem::path git_dir = std::filesystem::path(path) / ".git";
    return std::filesystem::exists(git_dir) && std::filesystem::is_directory(git_dir);
}

std::string get_git_root(const std::string& path) {
    if (!is_git_repo(path)) {
        throw std::runtime_error("Not a git repository: " + path);
    }

    std::string cmd = "cd \"" + path + "\" && git rev-parse --show-toplevel";
    std::string result = exec(cmd.c_str());
    
    // Remove trailing newline
    if (!result.empty() && result[result.length()-1] == '\n') {
        result.erase(result.length()-1);
    }
    
    return result;
}

std::vector<std::string> list_git_files(const std::string& repo_path) {
    if (!is_git_repo(repo_path)) {
        throw std::runtime_error("Not a git repository: " + repo_path);
    }

    std::string cmd = "cd \"" + repo_path + "\" && git ls-files";
    std::string output = exec(cmd.c_str());
    
    std::vector<std::string> files;
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            files.push_back(std::filesystem::path(repo_path) / line);
        }
    }
    
    return files;
}

bool is_file_tracked(const std::string& repo_path, const std::string& file_path) {
    std::string cmd = "cd \"" + repo_path + "\" && git ls-files --error-unmatch \"" + file_path + "\" 2>/dev/null";
    try {
        exec(cmd.c_str());
        return true;
    } catch (const std::runtime_error&) {
        return false;
    }
}

bool has_uncommitted_changes(const std::string& repo_path, const std::string& file_path) {
    if (!is_file_tracked(repo_path, file_path)) {
        return false;
    }

    std::string cmd = "cd \"" + repo_path + "\" && git diff --quiet HEAD \"" + file_path + "\"";
    int result = system(cmd.c_str());
    
    return result != 0;
}
} // namespace catchy::utils
