#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace icss::protocol {

enum class Transport : std::uint8_t {
    Tcp,
    Udp,
};

enum class TcpMessageKind : std::uint8_t {
    SessionCreate,
    SessionJoin,
    SessionLeave,
    ScenarioStart,
    ScenarioStop,
    ScenarioReset,
    TrackAcquire,
    TrackDrop,
    InterceptorReady,
    EngageOrder,
    CommandAck,
    AssessmentEvent,
    AarRequest,
    AarResponse,
};

enum class UdpMessageKind : std::uint8_t {
    WorldSnapshot,
    EntityState,
    TrackSummary,
    Telemetry,
    DisplayHeartbeat,
    DisplayHeartbeatAck,
};

enum class EventType : std::uint8_t {
    SessionStarted,
    SessionEnded,
    ClientJoined,
    ClientLeft,
    ClientReconnected,
    TrackUpdated,
    InterceptorUpdated,
    EngageOrderAccepted,
    EngageOrderRejected,
    AssessmentProduced,
    ResilienceTriggered,
};

struct SessionEnvelope {
    std::uint32_t session_id {};
    std::uint32_t sender_id {};
    std::uint64_t sequence {};
};

struct SnapshotHeader {
    std::uint64_t tick {};
    std::uint64_t timestamp_ms {};
    std::uint64_t snapshot_sequence {};
    std::uint64_t capture_wall_time_ms {};
};

struct TelemetrySample {
    std::uint64_t tick {};
    std::uint32_t tick_interval_ms {};
    float packet_loss_pct {};
    std::uint64_t last_snapshot_timestamp_ms {};
    std::uint64_t last_snapshot_wall_time_ms {};
};

struct EventRecordHeader {
    std::uint64_t tick {};
    std::uint64_t timestamp_ms {};
    EventType event_type {EventType::SessionStarted};
};

inline constexpr std::array<TcpMessageKind, 14> kTcpMessageKinds {
    TcpMessageKind::SessionCreate,
    TcpMessageKind::SessionJoin,
    TcpMessageKind::SessionLeave,
    TcpMessageKind::ScenarioStart,
    TcpMessageKind::ScenarioStop,
    TcpMessageKind::ScenarioReset,
    TcpMessageKind::TrackAcquire,
    TcpMessageKind::TrackDrop,
    TcpMessageKind::InterceptorReady,
    TcpMessageKind::EngageOrder,
    TcpMessageKind::CommandAck,
    TcpMessageKind::AssessmentEvent,
    TcpMessageKind::AarRequest,
    TcpMessageKind::AarResponse,
};

inline constexpr std::array<UdpMessageKind, 6> kUdpMessageKinds {
    UdpMessageKind::WorldSnapshot,
    UdpMessageKind::EntityState,
    UdpMessageKind::TrackSummary,
    UdpMessageKind::Telemetry,
    UdpMessageKind::DisplayHeartbeat,
    UdpMessageKind::DisplayHeartbeatAck,
};

inline constexpr std::array<EventType, 11> kEventTypes {
    EventType::SessionStarted,
    EventType::SessionEnded,
    EventType::ClientJoined,
    EventType::ClientLeft,
    EventType::ClientReconnected,
    EventType::TrackUpdated,
    EventType::InterceptorUpdated,
    EventType::EngageOrderAccepted,
    EventType::EngageOrderRejected,
    EventType::AssessmentProduced,
    EventType::ResilienceTriggered,
};

inline constexpr std::string_view to_string(Transport value) {
    switch (value) {
    case Transport::Tcp: return "tcp";
    case Transport::Udp: return "udp";
    }
    return "unknown";
}

inline constexpr std::string_view to_string(TcpMessageKind value) {
    switch (value) {
    case TcpMessageKind::SessionCreate: return "session_create";
    case TcpMessageKind::SessionJoin: return "session_join";
    case TcpMessageKind::SessionLeave: return "session_leave";
    case TcpMessageKind::ScenarioStart: return "scenario_start";
    case TcpMessageKind::ScenarioStop: return "scenario_stop";
    case TcpMessageKind::ScenarioReset: return "scenario_reset";
    case TcpMessageKind::TrackAcquire: return "track_acquire";
    case TcpMessageKind::TrackDrop: return "track_drop";
    case TcpMessageKind::InterceptorReady: return "interceptor_ready";
    case TcpMessageKind::EngageOrder: return "engage_order";
    case TcpMessageKind::CommandAck: return "command_ack";
    case TcpMessageKind::AssessmentEvent: return "assessment_event";
    case TcpMessageKind::AarRequest: return "aar_request";
    case TcpMessageKind::AarResponse: return "aar_response";
    }
    return "unknown_tcp_message";
}

inline constexpr std::string_view to_string(UdpMessageKind value) {
    switch (value) {
    case UdpMessageKind::WorldSnapshot: return "world_snapshot";
    case UdpMessageKind::EntityState: return "entity_state";
    case UdpMessageKind::TrackSummary: return "track_summary";
    case UdpMessageKind::Telemetry: return "telemetry";
    case UdpMessageKind::DisplayHeartbeat: return "display_heartbeat";
    case UdpMessageKind::DisplayHeartbeatAck: return "display_heartbeat_ack";
    }
    return "unknown_udp_message";
}

inline constexpr std::string_view to_string(EventType value) {
    switch (value) {
    case EventType::SessionStarted: return "session_started";
    case EventType::SessionEnded: return "session_ended";
    case EventType::ClientJoined: return "client_joined";
    case EventType::ClientLeft: return "client_left";
    case EventType::ClientReconnected: return "client_reconnected";
    case EventType::TrackUpdated: return "track_updated";
    case EventType::InterceptorUpdated: return "interceptor_updated";
    case EventType::EngageOrderAccepted: return "engage_order_accepted";
    case EventType::EngageOrderRejected: return "engage_order_rejected";
    case EventType::AssessmentProduced: return "assessment_produced";
    case EventType::ResilienceTriggered: return "resilience_triggered";
    }
    return "unknown_event_type";
}

}  // namespace icss::protocol
