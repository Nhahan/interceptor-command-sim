#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "icss/core/runtime.hpp"
#include "tests/support/temp_repo.hpp"

namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_runtime_artifact_determinism_");

    const auto run_once = [&](std::string& summary_md,
                              std::string& summary_json,
                              std::string& timeline_json,
                              std::string& sample_output,
                              std::string& runtime_log) {
        BaselineRuntime runtime(default_runtime_config(temp_root));
        const auto result = runtime.run();
        assert(result.summary.judgment_ready);

        summary_md = read_text(temp_root / "assets/sample-aar/session-summary.md");
        summary_json = read_text(temp_root / "assets/sample-aar/session-summary.json");
        timeline_json = read_text(temp_root / "assets/sample-aar/replay-timeline.json");
        sample_output = read_text(temp_root / "examples/sample-output.md");
        runtime_log = read_text(temp_root / "logs/session.log");
    };

    std::string summary_md_a;
    std::string summary_json_a;
    std::string timeline_json_a;
    std::string sample_output_a;
    std::string runtime_log_a;
    run_once(summary_md_a, summary_json_a, timeline_json_a, sample_output_a, runtime_log_a);

    std::string summary_md_b;
    std::string summary_json_b;
    std::string timeline_json_b;
    std::string sample_output_b;
    std::string runtime_log_b;
    run_once(summary_md_b, summary_json_b, timeline_json_b, sample_output_b, runtime_log_b);

    assert(summary_md_a == summary_md_b);
    assert(summary_json_a == summary_json_b);
    assert(timeline_json_a == timeline_json_b);
    assert(sample_output_a == sample_output_b);
    assert(runtime_log_a == runtime_log_b);

    fs::remove_all(temp_root);
    return 0;
}
