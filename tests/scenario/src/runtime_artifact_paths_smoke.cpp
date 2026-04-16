#include <cassert>
#include <filesystem>

#include "icss/core/runtime.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const fs::path temp_root = fs::temp_directory_path() / "icss_runtime_artifacts_test";
    fs::remove_all(temp_root);
    fs::create_directories(temp_root / "configs");

    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml", temp_root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml", temp_root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml", temp_root / "configs/logging.example.yaml");

    BaselineRuntime runtime(default_runtime_config(temp_root));
    const auto result = runtime.run();

    assert(result.summary.judgment_ready);
    assert(fs::exists(temp_root / "assets/sample-aar/replay-timeline.json"));
    assert(fs::exists(temp_root / "assets/sample-aar/session-summary.md"));
    assert(fs::exists(temp_root / "examples/sample-output.md"));
    assert(fs::exists(temp_root / "logs/session.log"));

    fs::remove_all(temp_root);
    return 0;
}
