#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace icss::testsupport {

inline std::filesystem::path make_temp_configured_repo(const std::string& prefix) {
    namespace fs = std::filesystem;

    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / (prefix + unique_suffix);

    fs::remove_all(root);
    fs::create_directories(root / "configs");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml",
                  root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml",
                  root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml",
                  root / "configs/logging.example.yaml");
    return root;
}

}  // namespace icss::testsupport
