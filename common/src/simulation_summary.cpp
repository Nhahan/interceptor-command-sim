#include "icss/core/simulation.hpp"

#include <filesystem>
#include <fstream>
#include <string_view>

#include "icss/view/ascii_tactical_view.hpp"
#include "simulation_internal.hpp"

namespace icss::core {
namespace {

constexpr std::string_view kReplayTimelineSchemaVersion = "icss-replay-timeline-v1";
constexpr std::string_view kSessionSummarySchemaVersion = "icss-session-summary-v1";
constexpr std::string_view kSessionSummaryJsonSchemaVersion = "icss-session-summary-json-v1";
constexpr std::string_view kSampleOutputSchemaVersion = "icss-sample-output-v1";

bool event_guidance_active(const EventRecord& event) {
    if (event.summary.find("disabled") != std::string::npos
        || event.details.find("disabled") != std::string::npos
        || event.summary.find("straight") != std::string::npos
        || event.details.find("straight") != std::string::npos) {
        return false;
    }
    return event.summary.find("enabled") != std::string::npos
        || event.details.find("enabled") != std::string::npos
        || event.summary.find("guided") != std::string::npos
        || event.details.find("guided") != std::string::npos;
}

bool artifact_guidance_active(const std::vector<EventRecord>& events, const Snapshot& snapshot) {
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        if (it->header.event_type == protocol::EventType::CommandAccepted
            || it->header.event_type == protocol::EventType::TrackUpdated) {
            return event_guidance_active(*it);
        }
    }
    return snapshot.track.active;
}

const char* guidance_state_label(bool guidance_active) {
    return guidance_active ? "on" : "off";
}

const char* launch_mode_label(bool guidance_active) {
    return guidance_active ? "guided" : "straight";
}

}  // namespace

SessionSummary SimulationSession::build_summary() const {
    return {
        session_id_,
        phase_,
        snapshots_.size(),
        events_.size(),
        command_console_.connection,
        tactical_viewer_.connection,
        judgment_.ready,
        judgment_.code,
        !events_.empty(),
        events_.empty() ? protocol::EventType::SessionStarted : events_.back().header.event_type,
        detail::build_resilience_case(reconnect_exercised_, timeout_exercised_, packet_gap_exercised_),
    };
}

void SimulationSession::write_aar_artifacts(const std::filesystem::path& output_dir) const {
    if (!output_dir.empty()) {
        std::filesystem::create_directories(output_dir);
    }
    const auto summary = build_summary();
    const auto snapshot = latest_snapshot();
    const bool guidance_active = artifact_guidance_active(events_, snapshot);

    std::ofstream timeline(output_dir / "replay-timeline.json");
    timeline << "{\n";
    timeline << "  \"schema_version\": \"" << kReplayTimelineSchemaVersion << "\",\n";
    timeline << "  \"session_id\": \"" << session_id_ << "\",\n";
    timeline << "  \"final_phase\": \"" << to_string(summary.phase) << "\",\n";
    timeline << "  \"snapshot_count\": " << summary.snapshot_count << ",\n";
    timeline << "  \"event_count\": " << summary.event_count << ",\n";
    timeline << "  \"judgment_code\": \"" << to_string(summary.judgment_code) << "\",\n";
    timeline << "  \"guidance_state\": \"" << guidance_state_label(guidance_active) << "\",\n";
    timeline << "  \"launch_mode\": \"" << launch_mode_label(guidance_active) << "\",\n";
    timeline << "  \"launch_angle_deg\": " << static_cast<int>(snapshot.launch_angle_deg) << ",\n";
    timeline << "  \"resilience_case\": \"" << detail::escape_json(summary.resilience_case) << "\",\n";
    timeline << "  \"events\": [\n";
    for (std::size_t index = 0; index < events_.size(); ++index) {
        const auto& event = events_[index];
        timeline << "    {\n";
        timeline << "      \"timestamp_ms\": " << event.header.timestamp_ms << ",\n";
        timeline << "      \"tick\": " << event.header.tick << ",\n";
        timeline << "      \"event_type\": \"" << protocol::to_string(event.header.event_type) << "\",\n";
        timeline << "      \"source\": \"" << detail::escape_json(event.source) << "\",\n";
        timeline << "      \"entity_ids\": [";
        for (std::size_t entity_index = 0; entity_index < event.entity_ids.size(); ++entity_index) {
            timeline << "\"" << detail::escape_json(event.entity_ids[entity_index]) << "\"";
            if (entity_index + 1 != event.entity_ids.size()) {
                timeline << ", ";
            }
        }
        timeline << "],\n";
        timeline << "      \"summary\": \"" << detail::escape_json(event.summary) << "\",\n";
        timeline << "      \"details\": \"" << detail::escape_json(event.details) << "\"\n";
        timeline << "    }" << (index + 1 == events_.size() ? "\n" : ",\n");
    }
    timeline << "  ]\n";
    timeline << "}\n";

    const auto recent_events = detail::recent_events_for_artifact(events_);

    std::ofstream summary_json(output_dir / "session-summary.json");
    summary_json << "{\n";
    summary_json << "  \"schema_version\": \"" << kSessionSummaryJsonSchemaVersion << "\",\n";
    summary_json << "  \"session_id\": " << summary.session_id << ",\n";
    summary_json << "  \"final_phase\": \"" << to_string(summary.phase) << "\",\n";
    summary_json << "  \"snapshot_count\": " << summary.snapshot_count << ",\n";
    summary_json << "  \"event_count\": " << summary.event_count << ",\n";
    summary_json << "  \"command_console_connection\": \"" << to_string(summary.command_console_connection) << "\",\n";
    summary_json << "  \"viewer_connection\": \"" << to_string(summary.viewer_connection) << "\",\n";
    summary_json << "  \"judgment_ready\": " << (summary.judgment_ready ? "true" : "false") << ",\n";
    summary_json << "  \"judgment_code\": \"" << to_string(summary.judgment_code) << "\",\n";
    summary_json << "  \"guidance_state\": \"" << guidance_state_label(guidance_active) << "\",\n";
    summary_json << "  \"launch_mode\": \"" << launch_mode_label(guidance_active) << "\",\n";
    summary_json << "  \"launch_angle_deg\": " << static_cast<int>(snapshot.launch_angle_deg) << ",\n";
    summary_json << "  \"last_event_type\": \"" << (summary.has_last_event ? protocol::to_string(summary.last_event_type) : "none") << "\",\n";
    summary_json << "  \"resilience_case\": \"" << detail::escape_json(summary.resilience_case) << "\",\n";
    summary_json << "  \"latest_snapshot_sequence\": " << snapshot.header.snapshot_sequence << ",\n";
    summary_json << "  \"latest_viewer_connection\": \"" << to_string(snapshot.viewer_connection) << "\",\n";
    summary_json << "  \"latest_freshness\": \"" << view::freshness_label(snapshot) << "\",\n";
    summary_json << "  \"recent_events\": [\n";
    for (std::size_t index = 0; index < recent_events.size(); ++index) {
        const auto& event = recent_events[index];
        summary_json << "    {\n";
        summary_json << "      \"tick\": " << event.header.tick << ",\n";
        summary_json << "      \"event_type\": \"" << protocol::to_string(event.header.event_type) << "\",\n";
        summary_json << "      \"summary\": \"" << detail::escape_json(event.summary) << "\"\n";
        summary_json << "    }" << (index + 1 == recent_events.size() ? "\n" : ",\n");
    }
    summary_json << "  ]\n";
    summary_json << "}\n";

    std::ofstream summary_file(output_dir / "session-summary.md");
    summary_file << "# Sample AAR Session Summary\n\n";
    summary_file << "## Summary\n\n";
    summary_file << "- schema_version: " << kSessionSummarySchemaVersion << "\n";
    summary_file << "- session_id: " << summary.session_id << "\n";
    summary_file << "- final_phase: " << to_string(summary.phase) << "\n";
    summary_file << "- snapshot_count: " << summary.snapshot_count << "\n";
    summary_file << "- event_count: " << summary.event_count << "\n";
    summary_file << "- command_console_connection: " << to_string(summary.command_console_connection) << "\n";
    summary_file << "- viewer_connection: " << to_string(summary.viewer_connection) << "\n";
    summary_file << "- judgment_ready: " << (summary.judgment_ready ? "true" : "false") << "\n";
    summary_file << "- judgment_code: " << to_string(summary.judgment_code) << "\n";
    summary_file << "- guidance_state: " << guidance_state_label(guidance_active) << "\n";
    summary_file << "- launch_mode: " << launch_mode_label(guidance_active) << "\n";
    summary_file << "- launch_angle_deg: " << static_cast<int>(snapshot.launch_angle_deg) << "\n";
    summary_file << "- last_event_type: " << (summary.has_last_event ? protocol::to_string(summary.last_event_type) : "none") << "\n";
    summary_file << "- resilience_case: " << summary.resilience_case << "\n";
    summary_file << "- latest_snapshot_sequence: " << snapshot.header.snapshot_sequence << "\n";
    summary_file << "- latest_viewer_connection: " << to_string(snapshot.viewer_connection) << "\n";
    summary_file << "- latest_freshness: " << view::freshness_label(snapshot) << "\n";
    summary_file << "\n## Recent Events\n\n";
    for (const auto& event : recent_events) {
        summary_file << "- [tick " << event.header.tick << "] "
                     << event.summary << " (" << protocol::to_string(event.header.event_type) << ")\n";
    }
}

void SimulationSession::write_example_output(const std::filesystem::path& output_file) const {
    const auto parent = output_file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(output_file);

    const auto summary = build_summary();
    const auto snapshot = latest_snapshot();
    const auto recent_events = detail::recent_events_for_artifact(events_);
    const auto cursor = view::make_replay_cursor(events_.size(), events_.empty() ? 0 : events_.size() - 1);
    const bool guidance_active = artifact_guidance_active(events_, snapshot);
    out << "# Sample Output\n\n";
    out << "- schema_version: " << kSampleOutputSchemaVersion << "\n";
    out << "- session_id: " << summary.session_id << "\n";
    out << "- cursor_index: " << cursor.index << "/" << cursor.total << "\n";
    out << "- command_console_connection: " << to_string(summary.command_console_connection) << "\n";
    out << "- viewer_connection: " << to_string(summary.viewer_connection) << "\n";
    out << "- guidance_state: " << guidance_state_label(guidance_active) << "\n";
    out << "- launch_mode: " << launch_mode_label(guidance_active) << "\n";
    out << "- launch_angle_deg: " << static_cast<int>(snapshot.launch_angle_deg) << "\n";
    out << "- latest_freshness: " << view::freshness_label(snapshot) << "\n";
    out << "- latest_snapshot_sequence: " << snapshot.header.snapshot_sequence << "\n";
    out << "- last_event_type: " << (summary.has_last_event ? protocol::to_string(summary.last_event_type) : "none") << "\n";
    out << "- resilience_case: " << summary.resilience_case << "\n\n";
    out << "```text\n";
    out << view::render_tactical_frame(snapshot, recent_events, cursor);
    out << "```\n";
}

}  // namespace icss::core
