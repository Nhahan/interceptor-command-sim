#pragma once

#include <string>

#include "icss/protocol/messages.hpp"

namespace icss::protocol {

struct SessionCreatePayload {
    std::uint32_t requested_sender_id {};
    std::string scenario_name;
};

struct SessionJoinPayload {
    SessionEnvelope envelope {};
    std::string client_role;
};

struct SessionLeavePayload {
    SessionEnvelope envelope {};
    std::string reason;
};

struct ScenarioStartPayload {
    SessionEnvelope envelope {};
    std::string scenario_name;
    int world_width {576};
    int world_height {384};
    int target_start_x {80};
    int target_start_y {300};
    int target_velocity_x {5};
    int target_velocity_y {-3};
    int interceptor_start_x {160};
    int interceptor_start_y {60};
    int interceptor_speed_per_tick {14};
    int intercept_radius {12};
    int engagement_timeout_ticks {26};
};

struct ScenarioStopPayload {
    SessionEnvelope envelope {};
    std::string reason;
};

struct ScenarioResetPayload {
    SessionEnvelope envelope {};
    std::string reason;
};

struct TrackRequestPayload {
    SessionEnvelope envelope {};
    std::string target_id;
};

struct AssetActivatePayload {
    SessionEnvelope envelope {};
    std::string asset_id;
};

struct CommandIssuePayload {
    SessionEnvelope envelope {};
    std::string asset_id;
    std::string target_id;
};

struct JudgmentPayload {
    SessionEnvelope envelope {};
    bool accepted {false};
    std::string outcome;
};

struct CommandAckPayload {
    SessionEnvelope envelope {};
    bool accepted {false};
    std::string reason;
};

struct AarResponsePayload {
    SessionEnvelope envelope {};
    std::uint64_t replay_cursor_index {};
    std::string control {"absolute"};
    std::uint64_t requested_index {};
    bool clamped {false};
    std::string judgment_code;
    std::string resilience_case;
    std::uint64_t total_events {};
    std::string event_type;
    std::string event_summary;
};

struct SnapshotPayload {
    SessionEnvelope envelope {};
    SnapshotHeader header {};
    std::string phase;
    int world_width {576};
    int world_height {384};
    std::string target_id;
    bool target_active {false};
    int target_x {0};
    int target_y {0};
    int target_velocity_x {0};
    int target_velocity_y {0};
    std::string asset_id;
    bool asset_active {false};
    int asset_x {0};
    int asset_y {0};
    int interceptor_speed_per_tick {0};
    int intercept_radius {0};
    int engagement_timeout_ticks {0};
    bool tracking_active {false};
    int track_confidence_pct {0};
    std::string asset_status;
    std::string command_status;
    bool judgment_ready {false};
    std::string judgment_code;
};

struct TelemetryPayload {
    SessionEnvelope envelope {};
    TelemetrySample sample {};
    std::string connection_state;
    std::uint64_t event_tick {};
    std::string event_type;
    std::string event_summary;
};

struct ViewerHeartbeatPayload {
    SessionEnvelope envelope {};
    std::uint64_t heartbeat_id {};
};

struct AarRequestPayload {
    SessionEnvelope envelope {};
    std::uint64_t replay_cursor_index {};
    std::string control {"absolute"};
};

}  // namespace icss::protocol
