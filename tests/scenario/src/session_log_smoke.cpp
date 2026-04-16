#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "icss/core/runtime.hpp"
#include "tests/support/temp_repo.hpp"

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

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_session_log_test_");

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
    bool saw_backend = false;
    bool saw_judgment_code = false;
    std::size_t record_count = 0;
    while (std::getline(in, line)) {
        assert(looks_like_json_record(line));
        saw_summary = saw_summary || line.find("\"record_type\":\"session_summary\"") != std::string::npos;
        saw_command_accepted = saw_command_accepted || line.find("\"event_type\":\"command_accepted\"") != std::string::npos;
        saw_resilience = saw_resilience || line.find("\"event_type\":\"resilience_triggered\"") != std::string::npos;
        saw_backend = saw_backend || line.find("\"backend\":\"in_process\"") != std::string::npos;
        saw_judgment_code = saw_judgment_code || line.find("\"judgment_code\":\"intercept_success\"") != std::string::npos;
        ++record_count;
    }
    assert(record_count >= 2);
    assert(saw_summary);
    assert(saw_command_accepted);
    assert(saw_resilience);
    assert(saw_backend);
    assert(saw_judgment_code);

    fs::remove_all(temp_root);
    return 0;
}
