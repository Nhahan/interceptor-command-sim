#include <chrono>
#include <cassert>
#include <filesystem>

#include "icss/core/runtime.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const auto original_cwd = fs::current_path();
    const auto temp_root = fs::temp_directory_path() / ("icss_example_output_relative_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_root);
    fs::current_path(temp_root);

    auto session = run_baseline_demo(default_runtime_config(fs::path {ICSS_REPO_ROOT}));
    session.write_example_output("flat-sample-output.md");
    assert(fs::exists(temp_root / "flat-sample-output.md"));

    fs::current_path(original_cwd);
    fs::remove_all(temp_root);
    return 0;
}
