#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "icss/core/runtime.hpp"
#include "icss/support/minijson.hpp"
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
    bool saw_schema_version = false;
    bool saw_command_accepted = false;
    bool saw_resilience = false;
    bool saw_backend = false;
    bool saw_judgment_code = false;
    bool saw_command_connection = false;
    bool saw_last_event_type = false;
    std::size_t record_count = 0;
    while (std::getline(in, line)) {
        assert(looks_like_json_record(line));
        const auto parsed = icss::testsupport::minijson::parse(line);
        assert(parsed.is_object());
        const auto& object = parsed.as_object();
        saw_schema_version = saw_schema_version
            || icss::testsupport::minijson::require_field(object, "schema_version").as_string() == "icss-runtime-log-v1";
        const auto& record_type = icss::testsupport::minijson::require_field(object, "record_type");
        assert(record_type.is_string());
        if (record_type.as_string() == "session_summary") {
            saw_summary = true;
            saw_backend = saw_backend || icss::testsupport::minijson::require_field(object, "backend").as_string() == "in_process";
            saw_judgment_code = saw_judgment_code || icss::testsupport::minijson::require_field(object, "judgment_code").as_string() == "intercept_success";
            saw_command_connection = saw_command_connection || icss::testsupport::minijson::require_field(object, "command_console_connection").as_string() == "connected";
            saw_last_event_type = saw_last_event_type || icss::testsupport::minijson::require_field(object, "last_event_type").as_string() == "session_ended";
            assert(icss::testsupport::minijson::require_field(object, "session_id").is_int());
            assert(icss::testsupport::minijson::require_field(object, "phase").is_string());
            assert(icss::testsupport::minijson::require_field(object, "snapshot_count").is_int());
            assert(icss::testsupport::minijson::require_field(object, "event_count").is_int());
            assert(icss::testsupport::minijson::require_field(object, "viewer_connection").is_string());
            assert(icss::testsupport::minijson::require_field(object, "resilience").is_string());
        }
        if (record_type.as_string() == "event") {
            const auto& event_type = icss::testsupport::minijson::require_field(object, "event_type");
            assert(event_type.is_string());
            saw_command_accepted = saw_command_accepted || event_type.as_string() == "command_accepted";
            saw_resilience = saw_resilience || event_type.as_string() == "resilience_triggered";
            assert(icss::testsupport::minijson::require_field(object, "tick").is_int());
            assert(icss::testsupport::minijson::require_field(object, "timestamp_ms").is_int());
            assert(icss::testsupport::minijson::require_field(object, "source").is_string());
            assert(icss::testsupport::minijson::require_field(object, "summary").is_string());
            assert(icss::testsupport::minijson::require_field(object, "details").is_string());
            assert(icss::testsupport::minijson::require_field(object, "entity_ids").is_array());
        }
        ++record_count;
    }
    assert(record_count >= 2);
    assert(saw_schema_version);
    assert(saw_summary);
    assert(saw_command_accepted);
    assert(saw_resilience);
    assert(saw_backend);
    assert(saw_judgment_code);
    assert(saw_command_connection);
    assert(saw_last_event_type);

    fs::remove_all(temp_root);
    return 0;
}
