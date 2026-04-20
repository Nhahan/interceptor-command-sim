#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "icss/core/runtime.hpp"
#include "icss/support/minijson.hpp"
#include "tests/support/temp_repo.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_runtime_artifacts_test_");

    BaselineRuntime runtime(default_runtime_config(temp_root));
    const auto result = runtime.run();

    assert(result.summary.assessment_ready);
    assert(fs::exists(temp_root / "assets/sample-aar/replay-timeline.json"));
    assert(fs::exists(temp_root / "assets/sample-aar/session-summary.json"));
    assert(fs::exists(temp_root / "assets/sample-aar/session-summary.md"));
    assert(fs::exists(temp_root / "examples/sample-output.md"));
    assert(fs::exists(temp_root / "logs/session.log"));

    std::ifstream summary_in(temp_root / "assets/sample-aar/session-summary.md");
    std::stringstream summary_buffer;
    summary_buffer << summary_in.rdbuf();
    const auto summary_text = summary_buffer.str();
    assert(summary_text.find("schema_version: icss-session-summary-v1") != std::string::npos);
    assert(summary_text.find("latest_snapshot_sequence:") != std::string::npos);
    assert(summary_text.find("latest_freshness:") != std::string::npos);
    assert(summary_text.find("fire_control_console_connection:") != std::string::npos);
    assert(summary_text.find("effective_track_state: tracked") != std::string::npos);
    assert(summary_text.find("intercept_profile: tracked_intercept") != std::string::npos);
    assert(summary_text.find("launch_angle_deg: 45") != std::string::npos);
    assert(summary_text.find("last_event_type:") != std::string::npos);
    assert(summary_text.find("## Recent Events") != std::string::npos);

    std::ifstream output_in(temp_root / "examples/sample-output.md");
    std::stringstream output_buffer;
    output_buffer << output_in.rdbuf();
    const auto output_text = output_buffer.str();
    assert(output_text.find("schema_version: icss-sample-output-v1") != std::string::npos);
    assert(output_text.find("backend: in_process") != std::string::npos);
    assert(output_text.find("latest_freshness:") != std::string::npos);
    assert(output_text.find("latest_snapshot_sequence:") != std::string::npos);
    assert(output_text.find("fire_control_console_connection:") != std::string::npos);
    assert(output_text.find("effective_track_state: tracked") != std::string::npos);
    assert(output_text.find("intercept_profile: tracked_intercept") != std::string::npos);
    assert(output_text.find("launch_angle_deg: 45") != std::string::npos);
    assert(output_text.find("last_event_type:") != std::string::npos);
    assert(output_text.find("freshness=") != std::string::npos);

    std::ifstream timeline_in(temp_root / "assets/sample-aar/replay-timeline.json");
    std::stringstream timeline_buffer;
    timeline_buffer << timeline_in.rdbuf();
    const auto timeline_text = timeline_buffer.str();
    const auto timeline_json = icss::testsupport::minijson::parse(timeline_text);
    assert(timeline_json.is_object());
    const auto& timeline_object = timeline_json.as_object();
    assert(icss::testsupport::minijson::require_field(timeline_object, "schema_version").as_string() == "icss-replay-timeline-v1");
    assert(icss::testsupport::minijson::require_field(timeline_object, "session_id").is_string());
    assert(icss::testsupport::minijson::require_field(timeline_object, "final_phase").is_string());
    assert(icss::testsupport::minijson::require_field(timeline_object, "snapshot_count").is_int());
    assert(icss::testsupport::minijson::require_field(timeline_object, "event_count").is_int());
    assert(icss::testsupport::minijson::require_field(timeline_object, "assessment_code").is_string());
    assert(icss::testsupport::minijson::require_field(timeline_object, "effective_track_state").as_string() == "tracked");
    assert(icss::testsupport::minijson::require_field(timeline_object, "intercept_profile").as_string() == "tracked_intercept");
    assert(icss::testsupport::minijson::require_field(timeline_object, "launch_angle_deg").as_int() == 45);
    assert(icss::testsupport::minijson::require_field(timeline_object, "resilience_case").is_string());
    const auto& events = icss::testsupport::minijson::require_field(timeline_object, "events");
    assert(events.is_array());
    assert(!events.as_array().empty());
    const auto& first_event = events.as_array().front();
    assert(first_event.is_object());
    const auto& first_event_object = first_event.as_object();
    assert(icss::testsupport::minijson::require_field(first_event_object, "timestamp_ms").is_int());
    assert(icss::testsupport::minijson::require_field(first_event_object, "tick").is_int());
    assert(icss::testsupport::minijson::require_field(first_event_object, "event_type").is_string());
    assert(icss::testsupport::minijson::require_field(first_event_object, "source").is_string());
    assert(icss::testsupport::minijson::require_field(first_event_object, "entity_ids").is_array());
    assert(icss::testsupport::minijson::require_field(first_event_object, "summary").is_string());
    assert(icss::testsupport::minijson::require_field(first_event_object, "details").is_string());

    std::ifstream summary_json_in(temp_root / "assets/sample-aar/session-summary.json");
    std::stringstream summary_json_buffer;
    summary_json_buffer << summary_json_in.rdbuf();
    const auto summary_json_text = summary_json_buffer.str();
    const auto summary_json = icss::testsupport::minijson::parse(summary_json_text);
    assert(summary_json.is_object());
    const auto& summary_json_object = summary_json.as_object();
    assert(icss::testsupport::minijson::require_field(summary_json_object, "schema_version").as_string() == "icss-session-summary-json-v1");
    assert(icss::testsupport::minijson::require_field(summary_json_object, "session_id").is_int());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "final_phase").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "snapshot_count").is_int());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "event_count").is_int());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "fire_control_console_connection").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "display_connection").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "assessment_ready").is_bool());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "assessment_code").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "effective_track_state").as_string() == "tracked");
    assert(icss::testsupport::minijson::require_field(summary_json_object, "intercept_profile").as_string() == "tracked_intercept");
    assert(icss::testsupport::minijson::require_field(summary_json_object, "launch_angle_deg").as_int() == 45);
    assert(icss::testsupport::minijson::require_field(summary_json_object, "last_event_type").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "resilience_case").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "latest_snapshot_sequence").is_int());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "latest_display_connection").is_string());
    assert(icss::testsupport::minijson::require_field(summary_json_object, "latest_freshness").is_string());
    const auto& recent = icss::testsupport::minijson::require_field(summary_json_object, "recent_events");
    assert(recent.is_array());
    assert(!recent.as_array().empty());

    fs::remove_all(temp_root);
    return 0;
}
