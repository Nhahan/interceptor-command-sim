#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "icss/core/runtime.hpp"
#include "tests/support/temp_repo.hpp"

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace {

std::string read_text(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

int run_command_to_file(const std::string& command) {
    return std::system(command.c_str());
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const auto temp_root = icss::testsupport::make_temp_configured_repo("icss_artifact_summary_cli_");
    BaselineRuntime guided_runtime(default_runtime_config(temp_root), SampleMode::Guided);
    const auto guided_result = guided_runtime.run();
    assert(guided_result.summary.judgment_ready);

    const auto straight_root = icss::testsupport::make_temp_configured_repo("icss_artifact_summary_cli_straight_");
    BaselineRuntime straight_runtime(default_runtime_config(straight_root), SampleMode::Straight);
    const auto straight_result = straight_runtime.run();
    assert(straight_result.summary.judgment_ready);

    fs::create_directories(temp_root / "assets/sample-aar/straight");
    fs::copy_file(straight_root / "assets/sample-aar/session-summary.json",
                  temp_root / "assets/sample-aar/straight/session-summary.json",
                  fs::copy_options::overwrite_existing);
    fs::copy_file(straight_root / "assets/sample-aar/replay-timeline.json",
                  temp_root / "assets/sample-aar/straight/replay-timeline.json",
                  fs::copy_options::overwrite_existing);

    const fs::path output_file = temp_root / "artifact-summary.txt";
    const std::string tool_path = (fs::path{ICSS_REPO_ROOT} / "build/icss_artifact_summary").string();
    const std::string repo_root_command =
        "\"" + tool_path + "\" --repo-root \"" + temp_root.string() + "\" > \"" + output_file.string() + "\" 2>&1";

    const int repo_root_result = run_command_to_file(repo_root_command);
#if !defined(_WIN32)
    assert(WIFEXITED(repo_root_result));
    assert(WEXITSTATUS(repo_root_result) == 0);
#else
    assert(repo_root_result == 0);
#endif

    const auto output = read_text(output_file);
    assert(output.find("ICSS Artifact Summary") != std::string::npos);
    assert(output.find("guided.summary.session_id=1001") != std::string::npos);
    assert(output.find("guided.summary.judgment_code=intercept_success") != std::string::npos);
    assert(output.find("guided.timeline.event_count=") != std::string::npos);
    assert(output.find("guided.log.backend=in_process") != std::string::npos);
    assert(output.find("straight.available=true") != std::string::npos);
    assert(output.find("straight.summary.judgment_code=timeout_observed") != std::string::npos);
    assert(output.find("compare.guided_judgment=intercept_success") != std::string::npos);
    assert(output.find("compare.straight_judgment=timeout_observed") != std::string::npos);
    assert(output.find("compare.guided_launch_mode=guided") != std::string::npos);
    assert(output.find("compare.straight_launch_mode=straight") != std::string::npos);

    const fs::path compare_output_file = temp_root / "artifact-summary-compare.txt";
    const std::string compare_command =
        "\"" + tool_path + "\" --guided-root \"" + temp_root.string()
            + "\" --straight-root \"" + straight_root.string()
            + "\" > \"" + compare_output_file.string() + "\" 2>&1";
    const int compare_result = run_command_to_file(compare_command);
#if !defined(_WIN32)
    assert(WIFEXITED(compare_result));
    assert(WEXITSTATUS(compare_result) == 0);
#else
    assert(compare_result == 0);
#endif
    const auto compare_output = read_text(compare_output_file);
    assert(compare_output.find("guided.summary.judgment_code=intercept_success") != std::string::npos);
    assert(compare_output.find("straight.summary.judgment_code=timeout_observed") != std::string::npos);
    assert(compare_output.find("compare.guided_judgment=intercept_success") != std::string::npos);
    assert(compare_output.find("compare.straight_judgment=timeout_observed") != std::string::npos);

    const fs::path invalid_output_file = temp_root / "artifact-summary-invalid.txt";
    const std::string partial_invalid_command =
        "\"" + tool_path + "\" --guided-root \"" + temp_root.string()
            + "\" > \"" + invalid_output_file.string() + "\" 2>&1";
    const int partial_invalid_result = run_command_to_file(partial_invalid_command);
#if !defined(_WIN32)
    assert(WIFEXITED(partial_invalid_result));
    assert(WEXITSTATUS(partial_invalid_result) != 0);
#else
    assert(partial_invalid_result != 0);
#endif
    const auto partial_invalid_output = read_text(invalid_output_file);
    assert(partial_invalid_output.find("--guided-root and --straight-root must be provided together") != std::string::npos);

    const fs::path mixed_output_file = temp_root / "artifact-summary-mixed-invalid.txt";
    const std::string mixed_invalid_command =
        "\"" + tool_path + "\" --repo-root \"" + temp_root.string()
            + "\" --guided-root \"" + temp_root.string()
            + "\" --straight-root \"" + straight_root.string()
            + "\" > \"" + mixed_output_file.string() + "\" 2>&1";
    const int mixed_invalid_result = run_command_to_file(mixed_invalid_command);
#if !defined(_WIN32)
    assert(WIFEXITED(mixed_invalid_result));
    assert(WEXITSTATUS(mixed_invalid_result) != 0);
#else
    assert(mixed_invalid_result != 0);
#endif
    const auto mixed_invalid_output = read_text(mixed_output_file);
    assert(mixed_invalid_output.find("--repo-root cannot be combined with compare-root flags") != std::string::npos);

    fs::remove_all(temp_root);
    fs::remove_all(straight_root);
    return 0;
}
