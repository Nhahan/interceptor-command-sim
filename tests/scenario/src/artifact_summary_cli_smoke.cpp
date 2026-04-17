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

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const auto temp_root = icss::testsupport::make_temp_configured_repo("icss_artifact_summary_cli_");
    BaselineRuntime runtime(default_runtime_config(temp_root));
    const auto result = runtime.run();
    assert(result.summary.judgment_ready);

    const fs::path output_file = temp_root / "artifact-summary.txt";
    const std::string tool_path = (fs::path{ICSS_REPO_ROOT} / "build/icss_artifact_summary").string();
    const std::string command =
        "\"" + tool_path + "\" --repo-root \"" + temp_root.string() + "\" > \"" + output_file.string() + "\" 2>&1";

    const int system_result = std::system(command.c_str());
#if !defined(_WIN32)
    assert(WIFEXITED(system_result));
    assert(WEXITSTATUS(system_result) == 0);
#else
    assert(system_result == 0);
#endif

    const auto output = read_text(output_file);
    assert(output.find("ICSS Artifact Summary") != std::string::npos);
    assert(output.find("summary.session_id=1001") != std::string::npos);
    assert(output.find("summary.judgment_code=intercept_success") != std::string::npos);
    assert(output.find("timeline.event_count=") != std::string::npos);
    assert(output.find("log.backend=in_process") != std::string::npos);

    fs::remove_all(temp_root);
    return 0;
}
