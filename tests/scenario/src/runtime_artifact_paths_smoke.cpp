#include <cassert>
#include <filesystem>

#include "icss/core/runtime.hpp"
#include "tests/support/temp_repo.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_runtime_artifacts_test_");

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
