#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace icss::core {

struct ArtifactRecentEvent {
    std::uint64_t tick {};
    std::string event_type;
    std::string summary;
};

struct SessionSummaryArtifact {
    std::string schema_version;
    std::uint32_t session_id {};
    std::string final_phase;
    std::size_t snapshot_count {};
    std::size_t event_count {};
    std::string command_console_connection;
    std::string viewer_connection;
    bool judgment_ready {false};
    std::string judgment_code;
    std::string last_event_type;
    std::string resilience_case;
    std::uint64_t latest_snapshot_sequence {};
    std::string latest_viewer_connection;
    std::string latest_freshness;
    std::vector<ArtifactRecentEvent> recent_events;
};

struct ReplayTimelineEvent {
    std::uint64_t timestamp_ms {};
    std::uint64_t tick {};
    std::string event_type;
    std::string source;
    std::vector<std::string> entity_ids;
    std::string summary;
    std::string details;
};

struct ReplayTimelineArtifact {
    std::string schema_version;
    std::string session_id;
    std::string final_phase;
    std::size_t snapshot_count {};
    std::size_t event_count {};
    std::string judgment_code;
    std::string resilience_case;
    std::vector<ReplayTimelineEvent> events;
};

struct RuntimeLogArtifact {
    std::string schema_version;
    std::string backend;
    std::uint32_t session_id {};
    std::string phase;
    std::size_t snapshot_count {};
    std::size_t event_count {};
    std::string command_console_connection;
    std::string viewer_connection;
    std::string judgment_code;
    std::string last_event_type;
    std::string resilience_case;
    std::size_t event_record_count {};
    std::string last_recorded_event_type;
};

SessionSummaryArtifact read_session_summary_json(const std::filesystem::path& path);
ReplayTimelineArtifact read_replay_timeline_json(const std::filesystem::path& path);
RuntimeLogArtifact read_runtime_log(const std::filesystem::path& path);

}  // namespace icss::core
