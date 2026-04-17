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
};

struct ScenarioStopPayload {
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
    std::string target_id;
    std::string asset_id;
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
