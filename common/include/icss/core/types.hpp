#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "icss/protocol/messages.hpp"

namespace icss::core {

inline constexpr std::uint32_t kServerSenderId = 1U;

enum class ClientRole : std::uint8_t {
    FireControlConsole,
    TacticalDisplay,
};

enum class ConnectionState : std::uint8_t {
    Disconnected,
    Connected,
    Reconnected,
    TimedOut,
};

enum class SessionPhase : std::uint8_t {
    Standby,
    Detecting,
    Tracking,
    InterceptorReady,
    EngageOrdered,
    Intercepting,
    Assessed,
    Archived,
};

enum class InterceptorStatus : std::uint8_t {
    Idle,
    Ready,
    Intercepting,
    Complete,
};

enum class EngageOrderStatus : std::uint8_t {
    None,
    Accepted,
    Executing,
    Completed,
    Rejected,
};

enum class AssessmentCode : std::uint8_t {
    Pending,
    InterceptSuccess,
    InvalidTransition,
    TimeoutObserved,
};

struct Vec2 {
    int x {};
    int y {};
};

struct Vec2f {
    float x {};
    float y {};
};

struct EntityState {
    std::string id;
    Vec2 position {};
    bool active {false};
};

struct ClientState {
    ClientRole role {ClientRole::FireControlConsole};
    ConnectionState connection {ConnectionState::Disconnected};
    std::uint32_t sender_id {};
    bool has_connected_before {false};
};

struct TrackState {
    bool active {false};
    Vec2f estimated_position {};
    Vec2f estimated_velocity {};
    Vec2f measurement_position {};
    bool measurement_valid {false};
    float measurement_residual_distance {0.0F};
    float covariance_trace {0.0F};
    std::uint32_t measurement_age_ticks {0};
    std::uint32_t missed_updates {0};
};

struct AssessmentState {
    bool ready {false};
    AssessmentCode code {AssessmentCode::Pending};
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
    SessionPhase phase {SessionPhase::Standby};
    int world_width {2304};
    int world_height {1536};
    EntityState target;
    EntityState interceptor;
    Vec2f target_world_position {};
    Vec2f interceptor_world_position {};
    int target_velocity_x {0};
    int target_velocity_y {0};
    Vec2f target_velocity {};
    Vec2f interceptor_velocity {};
    float target_heading_deg {0.0F};
    float interceptor_heading_deg {0.0F};
    int interceptor_speed_per_tick {0};
    float interceptor_acceleration_per_tick {0.0F};
    int intercept_radius {0};
    int engagement_timeout_ticks {0};
    float seeker_fov_deg {0.0F};
    bool seeker_lock {false};
    float off_boresight_deg {0.0F};
    bool predicted_intercept_valid {false};
    Vec2f predicted_intercept_position {};
    float time_to_intercept_s {0.0F};
    TrackState track;
    InterceptorStatus interceptor_status {InterceptorStatus::Idle};
    EngageOrderStatus engage_order_status {EngageOrderStatus::None};
    AssessmentState assessment;
    ConnectionState display_connection {ConnectionState::Disconnected};
    protocol::TelemetrySample telemetry {};
    float launch_angle_deg {45.0F};
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
    SessionPhase phase {SessionPhase::Standby};
    std::size_t snapshot_count {};
    std::size_t event_count {};
    ConnectionState fire_control_console_connection {ConnectionState::Disconnected};
    ConnectionState display_connection {ConnectionState::Disconnected};
    bool assessment_ready {false};
    AssessmentCode assessment_code {AssessmentCode::Pending};
    bool has_last_event {false};
    protocol::EventType last_event_type {protocol::EventType::SessionStarted};
    std::string resilience_case;
};

inline constexpr const char* to_string(ClientRole role) {
    switch (role) {
    case ClientRole::FireControlConsole: return "fire_control_console";
    case ClientRole::TacticalDisplay: return "tactical_display";
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
    case SessionPhase::Standby: return "standby";
    case SessionPhase::Detecting: return "detecting";
    case SessionPhase::Tracking: return "tracking";
    case SessionPhase::InterceptorReady: return "interceptor_ready";
    case SessionPhase::EngageOrdered: return "engage_ordered";
    case SessionPhase::Intercepting: return "intercepting";
    case SessionPhase::Assessed: return "assessed";
    case SessionPhase::Archived: return "archived";
    }
    return "unknown_session_phase";
}

inline constexpr const char* to_string(InterceptorStatus status) {
    switch (status) {
    case InterceptorStatus::Idle: return "idle";
    case InterceptorStatus::Ready: return "ready";
    case InterceptorStatus::Intercepting: return "intercepting";
    case InterceptorStatus::Complete: return "complete";
    }
    return "unknown_interceptor_status";
}

inline constexpr const char* to_string(EngageOrderStatus lifecycle) {
    switch (lifecycle) {
    case EngageOrderStatus::None: return "none";
    case EngageOrderStatus::Accepted: return "accepted";
    case EngageOrderStatus::Executing: return "executing";
    case EngageOrderStatus::Completed: return "completed";
    case EngageOrderStatus::Rejected: return "rejected";
    }
    return "unknown_command_lifecycle";
}

inline constexpr const char* to_string(AssessmentCode code) {
    switch (code) {
    case AssessmentCode::Pending: return "pending";
    case AssessmentCode::InterceptSuccess: return "intercept_success";
    case AssessmentCode::InvalidTransition: return "invalid_transition";
    case AssessmentCode::TimeoutObserved: return "timeout_observed";
    }
    return "unknown_assessment_code";
}

}  // namespace icss::core
