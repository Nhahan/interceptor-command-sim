#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "icss/core/runtime.hpp"

namespace {

bool looks_like_json_record(const std::string& line) {
    if (line.size() < 2 || line.front() != '{' || line.back() != '}') {
        return false;
    }
    bool in_string = false;
    bool escaped = false;
    for (char ch : line) {
        const auto uch = static_cast<unsigned char>(ch);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            continue;
        }
        if (uch < 0x20U && ch != '\t' && ch != '\r' && ch != '\n') {
            return false;
        }
    }
    return !in_string && !escaped;
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path temp_root = fs::temp_directory_path() / ("icss_session_log_test_" + unique_suffix);
    fs::remove_all(temp_root);
    fs::create_directories(temp_root / "configs");

    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml", temp_root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml", temp_root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml", temp_root / "configs/logging.example.yaml");

    BaselineRuntime runtime(default_runtime_config(temp_root));
    const auto result = runtime.run();
    assert(result.summary.judgment_ready);

    const fs::path log_file = temp_root / "logs/session.log";
    assert(fs::exists(log_file));

    std::ifstream in(log_file);
    std::string line;
    bool saw_summary = false;
    bool saw_command_accepted = false;
    bool saw_resilience = false;
    std::size_t record_count = 0;
    while (std::getline(in, line)) {
        assert(looks_like_json_record(line));
        saw_summary = saw_summary || line.find("\"record_type\":\"session_summary\"") != std::string::npos;
        saw_command_accepted = saw_command_accepted || line.find("\"event_type\":\"command_accepted\"") != std::string::npos;
        saw_resilience = saw_resilience || line.find("\"event_type\":\"resilience_triggered\"") != std::string::npos;
        ++record_count;
    }
    assert(record_count >= 2);
    assert(saw_summary);
    assert(saw_command_accepted);
    assert(saw_resilience);

    fs::remove_all(temp_root);
    return 0;
}
