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
    TrackRequest,
    AssetActivate,
    CommandIssue,
    CommandAck,
    JudgmentEvent,
    AarRequest,
};

enum class UdpMessageKind : std::uint8_t {
    WorldSnapshot,
    EntityState,
    TrackingSummary,
    Telemetry,
};

enum class EventType : std::uint8_t {
    SessionStarted,
    SessionEnded,
    ClientJoined,
    ClientLeft,
    ClientReconnected,
    TrackUpdated,
    AssetUpdated,
    CommandAccepted,
    CommandRejected,
    JudgmentProduced,
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
};

struct TelemetrySample {
    std::uint64_t tick {};
    std::uint32_t latency_ms {};
    float packet_loss_pct {};
    std::uint64_t last_snapshot_timestamp_ms {};
};

struct EventRecordHeader {
    std::uint64_t tick {};
    std::uint64_t timestamp_ms {};
    EventType event_type {EventType::SessionStarted};
};

inline constexpr std::array<TcpMessageKind, 11> kTcpMessageKinds {
    TcpMessageKind::SessionCreate,
    TcpMessageKind::SessionJoin,
    TcpMessageKind::SessionLeave,
    TcpMessageKind::ScenarioStart,
    TcpMessageKind::ScenarioStop,
    TcpMessageKind::TrackRequest,
    TcpMessageKind::AssetActivate,
    TcpMessageKind::CommandIssue,
    TcpMessageKind::CommandAck,
    TcpMessageKind::JudgmentEvent,
    TcpMessageKind::AarRequest,
};

inline constexpr std::array<UdpMessageKind, 4> kUdpMessageKinds {
    UdpMessageKind::WorldSnapshot,
    UdpMessageKind::EntityState,
    UdpMessageKind::TrackingSummary,
    UdpMessageKind::Telemetry,
};

inline constexpr std::array<EventType, 11> kEventTypes {
    EventType::SessionStarted,
    EventType::SessionEnded,
    EventType::ClientJoined,
    EventType::ClientLeft,
    EventType::ClientReconnected,
    EventType::TrackUpdated,
    EventType::AssetUpdated,
    EventType::CommandAccepted,
    EventType::CommandRejected,
    EventType::JudgmentProduced,
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
    case TcpMessageKind::TrackRequest: return "track_request";
    case TcpMessageKind::AssetActivate: return "asset_activate";
    case TcpMessageKind::CommandIssue: return "command_issue";
    case TcpMessageKind::CommandAck: return "command_ack";
    case TcpMessageKind::JudgmentEvent: return "judgment_event";
    case TcpMessageKind::AarRequest: return "aar_request";
    }
    return "unknown_tcp_message";
}

inline constexpr std::string_view to_string(UdpMessageKind value) {
    switch (value) {
    case UdpMessageKind::WorldSnapshot: return "world_snapshot";
    case UdpMessageKind::EntityState: return "entity_state";
    case UdpMessageKind::TrackingSummary: return "tracking_summary";
    case UdpMessageKind::Telemetry: return "telemetry";
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
    case EventType::AssetUpdated: return "asset_updated";
    case EventType::CommandAccepted: return "command_accepted";
    case EventType::CommandRejected: return "command_rejected";
    case EventType::JudgmentProduced: return "judgment_produced";
    case EventType::ResilienceTriggered: return "resilience_triggered";
    }
    return "unknown_event_type";
}

}  // namespace icss::protocol
