#include "icss/core/artifact_readers.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "icss/support/minijson.hpp"

namespace icss::core {
namespace {

using icss::testsupport::minijson::Object;
using icss::testsupport::minijson::Value;

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open artifact: " + path.string());
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::size_t to_size(const Value& value) {
    return static_cast<std::size_t>(value.as_int());
}

std::uint64_t to_u64(const Value& value) {
    return static_cast<std::uint64_t>(value.as_int());
}

std::uint32_t to_u32(const Value& value) {
    return static_cast<std::uint32_t>(value.as_int());
}

}  // namespace

SessionSummaryArtifact read_session_summary_json(const std::filesystem::path& path) {
    const auto parsed = icss::testsupport::minijson::parse(read_text(path));
    const auto& object = parsed.as_object();

    SessionSummaryArtifact artifact;
    artifact.schema_version = icss::testsupport::minijson::require_field(object, "schema_version").as_string();
    artifact.session_id = to_u32(icss::testsupport::minijson::require_field(object, "session_id"));
    artifact.final_phase = icss::testsupport::minijson::require_field(object, "final_phase").as_string();
    artifact.snapshot_count = to_size(icss::testsupport::minijson::require_field(object, "snapshot_count"));
    artifact.event_count = to_size(icss::testsupport::minijson::require_field(object, "event_count"));
    artifact.fire_control_console_connection = icss::testsupport::minijson::require_field(object, "fire_control_console_connection").as_string();
    artifact.display_connection = icss::testsupport::minijson::require_field(object, "display_connection").as_string();
    artifact.assessment_ready = icss::testsupport::minijson::require_field(object, "assessment_ready").as_bool();
    artifact.assessment_code = icss::testsupport::minijson::require_field(object, "assessment_code").as_string();
    artifact.effective_track_state = icss::testsupport::minijson::require_field(object, "effective_track_state").as_string();
    artifact.intercept_profile = icss::testsupport::minijson::require_field(object, "intercept_profile").as_string();
    artifact.launch_angle_deg = to_u32(icss::testsupport::minijson::require_field(object, "launch_angle_deg"));
    artifact.last_event_type = icss::testsupport::minijson::require_field(object, "last_event_type").as_string();
    artifact.resilience_case = icss::testsupport::minijson::require_field(object, "resilience_case").as_string();
    artifact.latest_snapshot_sequence = to_u64(icss::testsupport::minijson::require_field(object, "latest_snapshot_sequence"));
    artifact.latest_display_connection = icss::testsupport::minijson::require_field(object, "latest_display_connection").as_string();
    artifact.latest_freshness = icss::testsupport::minijson::require_field(object, "latest_freshness").as_string();

    const auto& recent = icss::testsupport::minijson::require_field(object, "recent_events").as_array();
    for (const auto& entry : recent) {
        const auto& event_object = entry.as_object();
        artifact.recent_events.push_back({
            to_u64(icss::testsupport::minijson::require_field(event_object, "tick")),
            icss::testsupport::minijson::require_field(event_object, "event_type").as_string(),
            icss::testsupport::minijson::require_field(event_object, "summary").as_string(),
        });
    }

    return artifact;
}

ReplayTimelineArtifact read_replay_timeline_json(const std::filesystem::path& path) {
    const auto parsed = icss::testsupport::minijson::parse(read_text(path));
    const auto& object = parsed.as_object();

    ReplayTimelineArtifact artifact;
    artifact.schema_version = icss::testsupport::minijson::require_field(object, "schema_version").as_string();
    artifact.session_id = icss::testsupport::minijson::require_field(object, "session_id").as_string();
    artifact.final_phase = icss::testsupport::minijson::require_field(object, "final_phase").as_string();
    artifact.snapshot_count = to_size(icss::testsupport::minijson::require_field(object, "snapshot_count"));
    artifact.event_count = to_size(icss::testsupport::minijson::require_field(object, "event_count"));
    artifact.assessment_code = icss::testsupport::minijson::require_field(object, "assessment_code").as_string();
    artifact.effective_track_state = icss::testsupport::minijson::require_field(object, "effective_track_state").as_string();
    artifact.intercept_profile = icss::testsupport::minijson::require_field(object, "intercept_profile").as_string();
    artifact.launch_angle_deg = to_u32(icss::testsupport::minijson::require_field(object, "launch_angle_deg"));
    artifact.resilience_case = icss::testsupport::minijson::require_field(object, "resilience_case").as_string();

    const auto& events = icss::testsupport::minijson::require_field(object, "events").as_array();
    for (const auto& entry : events) {
        const auto& event_object = entry.as_object();
        ReplayTimelineEvent event;
        event.timestamp_ms = to_u64(icss::testsupport::minijson::require_field(event_object, "timestamp_ms"));
        event.tick = to_u64(icss::testsupport::minijson::require_field(event_object, "tick"));
        event.event_type = icss::testsupport::minijson::require_field(event_object, "event_type").as_string();
        event.source = icss::testsupport::minijson::require_field(event_object, "source").as_string();
        event.summary = icss::testsupport::minijson::require_field(event_object, "summary").as_string();
        event.details = icss::testsupport::minijson::require_field(event_object, "details").as_string();
        const auto& entity_ids = icss::testsupport::minijson::require_field(event_object, "entity_ids").as_array();
        for (const auto& entity_id : entity_ids) {
            event.entity_ids.push_back(entity_id.as_string());
        }
        artifact.events.push_back(std::move(event));
    }

    return artifact;
}

RuntimeLogArtifact read_runtime_log(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open runtime log: " + path.string());
    }

    RuntimeLogArtifact artifact;
    std::string line;
    while (std::getline(in, line)) {
        const auto parsed = icss::testsupport::minijson::parse(line);
        const auto& object = parsed.as_object();
        const auto record_type = icss::testsupport::minijson::require_field(object, "record_type").as_string();
        artifact.schema_version = icss::testsupport::minijson::require_field(object, "schema_version").as_string();
        if (record_type == "session_summary") {
            artifact.backend = icss::testsupport::minijson::require_field(object, "backend").as_string();
            artifact.session_id = to_u32(icss::testsupport::minijson::require_field(object, "session_id"));
            artifact.phase = icss::testsupport::minijson::require_field(object, "phase").as_string();
            artifact.snapshot_count = to_size(icss::testsupport::minijson::require_field(object, "snapshot_count"));
            artifact.event_count = to_size(icss::testsupport::minijson::require_field(object, "event_count"));
            artifact.fire_control_console_connection = icss::testsupport::minijson::require_field(object, "fire_control_console_connection").as_string();
            artifact.display_connection = icss::testsupport::minijson::require_field(object, "display_connection").as_string();
            artifact.assessment_code = icss::testsupport::minijson::require_field(object, "assessment_code").as_string();
            artifact.effective_track_state = icss::testsupport::minijson::require_field(object, "effective_track_state").as_string();
            artifact.intercept_profile = icss::testsupport::minijson::require_field(object, "intercept_profile").as_string();
            artifact.launch_angle_deg = to_u32(icss::testsupport::minijson::require_field(object, "launch_angle_deg"));
            artifact.last_event_type = icss::testsupport::minijson::require_field(object, "last_event_type").as_string();
            artifact.resilience_case = icss::testsupport::minijson::require_field(object, "resilience").as_string();
            continue;
        }
        if (record_type == "event") {
            ++artifact.event_record_count;
            artifact.last_recorded_event_type = icss::testsupport::minijson::require_field(object, "event_type").as_string();
        }
    }

    return artifact;
}

}  // namespace icss::core
