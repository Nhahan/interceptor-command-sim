#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "icss/protocol/messages.hpp"

namespace icss::core {

inline constexpr std::uint32_t kServerSenderId = 1U;

enum class ClientRole : std::uint8_t {
    CommandConsole,
    TacticalViewer,
};

enum class ConnectionState : std::uint8_t {
    Disconnected,
    Connected,
    Reconnected,
    TimedOut,
};

enum class SessionPhase : std::uint8_t {
    Initialized,
    Detecting,
    Tracking,
    AssetReady,
    CommandIssued,
    Engaging,
    Judged,
    Archived,
};

enum class AssetStatus : std::uint8_t {
    Idle,
    Ready,
    Engaging,
    Complete,
};

enum class CommandLifecycle : std::uint8_t {
    None,
    Accepted,
    Executing,
    Completed,
    Rejected,
};

enum class JudgmentCode : std::uint8_t {
    Pending,
    InterceptSuccess,
    InvalidTransition,
    TimeoutObserved,
};

struct Vec2 {
    int x {};
    int y {};
};

struct EntityState {
    std::string id;
    Vec2 position {};
    bool active {false};
};

struct ClientState {
    ClientRole role {ClientRole::CommandConsole};
    ConnectionState connection {ConnectionState::Disconnected};
    std::uint32_t sender_id {};
    std::uint64_t last_seen_tick {};
    bool has_connected_before {false};
};

struct TrackState {
    bool active {false};
    int confidence_pct {0};
};

struct JudgmentState {
    bool ready {false};
    JudgmentCode code {JudgmentCode::Pending};
    std::string summary;
};

struct CommandResult {
    bool accepted {false};
    std::string reason;
    protocol::TcpMessageKind ack_kind {protocol::TcpMessageKind::CommandAck};
};

struct Snapshot {
    protocol::SessionEnvelope envelope {};
    protocol::SnapshotHeader header {};
    SessionPhase phase {SessionPhase::Initialized};
    int world_width {576};
    int world_height {384};
    EntityState target;
    EntityState asset;
    int target_velocity_x {0};
    int target_velocity_y {0};
    int interceptor_speed_per_tick {0};
    int intercept_radius {0};
    int engagement_timeout_ticks {0};
    TrackState track;
    AssetStatus asset_status {AssetStatus::Idle};
    CommandLifecycle command_status {CommandLifecycle::None};
    JudgmentState judgment;
    ConnectionState viewer_connection {ConnectionState::Disconnected};
    protocol::TelemetrySample telemetry {};
};

struct EventRecord {
    protocol::EventRecordHeader header {};
    std::string source;
    std::vector<std::string> entity_ids;
    std::string summary;
    std::string details;
};

struct SessionSummary {
    std::uint32_t session_id {};
    SessionPhase phase {SessionPhase::Initialized};
    std::size_t snapshot_count {};
    std::size_t event_count {};
    ConnectionState command_console_connection {ConnectionState::Disconnected};
    ConnectionState viewer_connection {ConnectionState::Disconnected};
    bool judgment_ready {false};
    JudgmentCode judgment_code {JudgmentCode::Pending};
    bool has_last_event {false};
    protocol::EventType last_event_type {protocol::EventType::SessionStarted};
    std::string resilience_case;
};

inline constexpr const char* to_string(ClientRole role) {
    switch (role) {
    case ClientRole::CommandConsole: return "command_console";
    case ClientRole::TacticalViewer: return "tactical_viewer";
    }
    return "unknown_client_role";
}

inline constexpr const char* to_string(ConnectionState state) {
    switch (state) {
    case ConnectionState::Disconnected: return "disconnected";
    case ConnectionState::Connected: return "connected";
    case ConnectionState::Reconnected: return "reconnected";
    case ConnectionState::TimedOut: return "timed_out";
    }
    return "unknown_connection_state";
}

inline constexpr const char* to_string(SessionPhase phase) {
    switch (phase) {
    case SessionPhase::Initialized: return "initialized";
    case SessionPhase::Detecting: return "detecting";
    case SessionPhase::Tracking: return "tracking";
    case SessionPhase::AssetReady: return "asset_ready";
    case SessionPhase::CommandIssued: return "command_issued";
    case SessionPhase::Engaging: return "engaging";
    case SessionPhase::Judged: return "judged";
    case SessionPhase::Archived: return "archived";
    }
    return "unknown_session_phase";
}

inline constexpr const char* to_string(AssetStatus status) {
    switch (status) {
    case AssetStatus::Idle: return "idle";
    case AssetStatus::Ready: return "ready";
    case AssetStatus::Engaging: return "engaging";
    case AssetStatus::Complete: return "complete";
    }
    return "unknown_asset_status";
}

inline constexpr const char* to_string(CommandLifecycle lifecycle) {
    switch (lifecycle) {
    case CommandLifecycle::None: return "none";
    case CommandLifecycle::Accepted: return "accepted";
    case CommandLifecycle::Executing: return "executing";
    case CommandLifecycle::Completed: return "completed";
    case CommandLifecycle::Rejected: return "rejected";
    }
    return "unknown_command_lifecycle";
}

inline constexpr const char* to_string(JudgmentCode code) {
    switch (code) {
    case JudgmentCode::Pending: return "pending";
    case JudgmentCode::InterceptSuccess: return "intercept_success";
    case JudgmentCode::InvalidTransition: return "invalid_transition";
    case JudgmentCode::TimeoutObserved: return "timeout_observed";
    }
    return "unknown_judgment_code";
}

}  // namespace icss::core
