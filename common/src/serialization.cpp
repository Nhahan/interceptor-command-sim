#include "icss/protocol/serialization.hpp"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace icss::protocol {
namespace {

using FieldMap = std::unordered_map<std::string, std::string>;

std::string encode_field_value(std::string_view value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (unsigned char ch : value) {
        if (ch == '%' || ch == ';' || ch == '=' || ch < 0x20U) {
            encoded += '%';
            encoded += kHex[(ch >> 4U) & 0x0FU];
            encoded += kHex[ch & 0x0FU];
        } else {
            encoded += static_cast<char>(ch);
        }
    }
    return encoded;
}

std::string decode_field_value(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            auto hex = [](char c) -> unsigned char {
                if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
                if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(10 + (c - 'A'));
                if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(10 + (c - 'a'));
                throw std::runtime_error("invalid percent-encoding");
            };
            const unsigned char byte = static_cast<unsigned char>((hex(value[i + 1]) << 4U) | hex(value[i + 2]));
            decoded += static_cast<char>(byte);
            i += 2;
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}

std::vector<std::string_view> split(std::string_view text, char delim) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(delim, start);
        if (end == std::string_view::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

FieldMap parse_fields(std::string_view wire) {
    FieldMap fields;
    for (const auto part : split(wire, ';')) {
        if (part.empty()) {
            continue;
        }
        const auto pos = part.find('=');
        if (pos == std::string_view::npos) {
            throw std::runtime_error("invalid wire field: missing '='");
        }
        fields.emplace(std::string(part.substr(0, pos)), decode_field_value(part.substr(pos + 1)));
    }
    return fields;
}

std::string join_fields(std::initializer_list<std::pair<std::string_view, std::string>> fields) {
    std::string wire;
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) {
            wire += ';';
        }
        first = false;
        wire += key;
        wire += '=';
        wire += encode_field_value(value);
    }
    return wire;
}

std::string as_string(std::uint64_t value) {
    return std::to_string(value);
}

std::string as_string(std::uint32_t value) {
    return std::to_string(value);
}

std::string as_string(bool value) {
    return value ? "true" : "false";
}

std::string as_string(float value) {
    return std::to_string(value);
}

std::uint64_t parse_u64(std::string_view text) {
    std::uint64_t value {};
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {}) {
        throw std::runtime_error("invalid uint64 field");
    }
    return value;
}

std::uint32_t parse_u32(std::string_view text) {
    std::uint32_t value {};
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {}) {
        throw std::runtime_error("invalid uint32 field");
    }
    return value;
}

float parse_float(std::string_view text) {
    return std::stof(std::string(text));
}

bool parse_bool(std::string_view text) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    throw std::runtime_error("invalid bool field");
}

std::string require(const FieldMap& fields, const std::string& key) {
    const auto it = fields.find(key);
    if (it == fields.end()) {
        throw std::runtime_error("missing field: " + key);
    }
    return it->second;
}

void require_kind(const FieldMap& fields, std::string_view expected) {
    if (require(fields, "kind") != expected) {
        throw std::runtime_error("unexpected payload kind");
    }
}

SessionEnvelope parse_envelope(const FieldMap& fields) {
    return {
        parse_u32(require(fields, "session_id")),
        parse_u32(require(fields, "sender_id")),
        parse_u64(require(fields, "sequence")),
    };
}

SnapshotHeader parse_snapshot_header(const FieldMap& fields) {
    return {
        parse_u64(require(fields, "tick")),
        parse_u64(require(fields, "timestamp_ms")),
        parse_u64(require(fields, "snapshot_sequence")),
    };
}

TelemetrySample parse_telemetry_sample(const FieldMap& fields) {
    return {
        parse_u64(require(fields, "tick")),
        parse_u32(require(fields, "latency_ms")),
        parse_float(require(fields, "packet_loss_pct")),
        parse_u64(require(fields, "last_snapshot_timestamp_ms")),
    };
}

template <typename Predicate>
std::string require_enum_string(const FieldMap& fields, const std::string& key, Predicate is_valid) {
    const auto value = require(fields, key);
    if (!is_valid(value)) {
        throw std::runtime_error("invalid enum field: " + key);
    }
    return value;
}

bool is_valid_asset_status(std::string_view value) {
    return value == "idle" || value == "ready" || value == "engaging" || value == "complete";
}

bool is_valid_command_status(std::string_view value) {
    return value == "none" || value == "accepted" || value == "executing" || value == "completed" || value == "rejected";
}

bool is_valid_judgment_code(std::string_view value) {
    return value == "pending" || value == "intercept_success" || value == "invalid_transition" || value == "timeout_observed";
}

bool is_valid_aar_control(std::string_view value) {
    return value == "absolute" || value == "step_forward" || value == "step_backward";
}

}  // namespace

std::string serialize(const SessionCreatePayload& payload) {
    return join_fields({
        {"kind", "session_create"},
        {"requested_sender_id", as_string(payload.requested_sender_id)},
        {"scenario_name", payload.scenario_name},
    });
}

std::string serialize(const SessionJoinPayload& payload) {
    return join_fields({
        {"kind", "session_join"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"client_role", payload.client_role},
    });
}

std::string serialize(const SessionLeavePayload& payload) {
    return join_fields({
        {"kind", "session_leave"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"reason", payload.reason},
    });
}

std::string serialize(const ScenarioStartPayload& payload) {
    return join_fields({
        {"kind", "scenario_start"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"scenario_name", payload.scenario_name},
    });
}

std::string serialize(const ScenarioStopPayload& payload) {
    return join_fields({
        {"kind", "scenario_stop"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"reason", payload.reason},
    });
}

std::string serialize(const TrackRequestPayload& payload) {
    return join_fields({
        {"kind", "track_request"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"target_id", payload.target_id},
    });
}

std::string serialize(const AssetActivatePayload& payload) {
    return join_fields({
        {"kind", "asset_activate"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"asset_id", payload.asset_id},
    });
}

std::string serialize(const CommandIssuePayload& payload) {
    return join_fields({
        {"kind", "command_issue"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"asset_id", payload.asset_id},
        {"target_id", payload.target_id},
    });
}

std::string serialize(const JudgmentPayload& payload) {
    return join_fields({
        {"kind", "judgment"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"accepted", as_string(payload.accepted)},
        {"outcome", payload.outcome},
    });
}

std::string serialize(const CommandAckPayload& payload) {
    return join_fields({
        {"kind", "command_ack"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"accepted", as_string(payload.accepted)},
        {"reason", payload.reason},
    });
}

std::string serialize(const AarResponsePayload& payload) {
    return join_fields({
        {"kind", "aar_response"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"replay_cursor_index", as_string(payload.replay_cursor_index)},
        {"control", payload.control},
        {"requested_index", as_string(payload.requested_index)},
        {"clamped", as_string(payload.clamped)},
        {"judgment_code", payload.judgment_code},
        {"resilience_case", payload.resilience_case},
        {"total_events", as_string(static_cast<std::uint64_t>(payload.total_events))},
        {"event_type", payload.event_type},
        {"event_summary", payload.event_summary},
    });
}

std::string serialize(const SnapshotPayload& payload) {
    return join_fields({
        {"kind", "world_snapshot"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"tick", as_string(payload.header.tick)},
        {"timestamp_ms", as_string(payload.header.timestamp_ms)},
        {"snapshot_sequence", as_string(payload.header.snapshot_sequence)},
        {"target_id", payload.target_id},
        {"asset_id", payload.asset_id},
        {"tracking_active", as_string(payload.tracking_active)},
        {"track_confidence_pct", as_string(static_cast<std::uint32_t>(payload.track_confidence_pct))},
        {"asset_status", payload.asset_status},
        {"command_status", payload.command_status},
        {"judgment_ready", as_string(payload.judgment_ready)},
        {"judgment_code", payload.judgment_code},
    });
}

std::string serialize(const TelemetryPayload& payload) {
    return join_fields({
        {"kind", "telemetry"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"tick", as_string(payload.sample.tick)},
        {"latency_ms", as_string(payload.sample.latency_ms)},
        {"packet_loss_pct", as_string(payload.sample.packet_loss_pct)},
        {"last_snapshot_timestamp_ms", as_string(payload.sample.last_snapshot_timestamp_ms)},
        {"connection_state", payload.connection_state},
    });
}

std::string serialize(const ViewerHeartbeatPayload& payload) {
    return join_fields({
        {"kind", "viewer_heartbeat"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"heartbeat_id", as_string(payload.heartbeat_id)},
    });
}

std::string serialize(const AarRequestPayload& payload) {
    return join_fields({
        {"kind", "aar_request"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"replay_cursor_index", as_string(payload.replay_cursor_index)},
        {"control", payload.control},
    });
}

SessionCreatePayload parse_session_create(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "session_create");
    return {parse_u32(require(fields, "requested_sender_id")), require(fields, "scenario_name")};
}

SessionJoinPayload parse_session_join(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "session_join");
    return {parse_envelope(fields), require(fields, "client_role")};
}

SessionLeavePayload parse_session_leave(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "session_leave");
    return {parse_envelope(fields), require(fields, "reason")};
}

ScenarioStartPayload parse_scenario_start(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "scenario_start");
    return {parse_envelope(fields), require(fields, "scenario_name")};
}

ScenarioStopPayload parse_scenario_stop(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "scenario_stop");
    return {parse_envelope(fields), require(fields, "reason")};
}

TrackRequestPayload parse_track_request(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "track_request");
    return {parse_envelope(fields), require(fields, "target_id")};
}

AssetActivatePayload parse_asset_activate(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "asset_activate");
    return {parse_envelope(fields), require(fields, "asset_id")};
}

CommandIssuePayload parse_command_issue(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "command_issue");
    return {parse_envelope(fields), require(fields, "asset_id"), require(fields, "target_id")};
}

JudgmentPayload parse_judgment(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "judgment");
    return {parse_envelope(fields), parse_bool(require(fields, "accepted")), require(fields, "outcome")};
}

CommandAckPayload parse_command_ack(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "command_ack");
    return {parse_envelope(fields), parse_bool(require(fields, "accepted")), require(fields, "reason")};
}

AarResponsePayload parse_aar_response(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "aar_response");
    return {parse_envelope(fields), parse_u64(require(fields, "replay_cursor_index")),
            require_enum_string(fields, "control", is_valid_aar_control),
            parse_u64(require(fields, "requested_index")),
            parse_bool(require(fields, "clamped")),
            require_enum_string(fields, "judgment_code", is_valid_judgment_code),
            require(fields, "resilience_case"),
            parse_u64(require(fields, "total_events")),
            require(fields, "event_type"),
            require(fields, "event_summary")};
}

SnapshotPayload parse_snapshot(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "world_snapshot");
    return {
        parse_envelope(fields),
        parse_snapshot_header(fields),
        require(fields, "target_id"),
        require(fields, "asset_id"),
        parse_bool(require(fields, "tracking_active")),
        static_cast<int>(parse_u32(require(fields, "track_confidence_pct"))),
        require_enum_string(fields, "asset_status", is_valid_asset_status),
        require_enum_string(fields, "command_status", is_valid_command_status),
        parse_bool(require(fields, "judgment_ready")),
        require_enum_string(fields, "judgment_code", is_valid_judgment_code),
    };
}

TelemetryPayload parse_telemetry(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "telemetry");
    return {parse_envelope(fields), parse_telemetry_sample(fields), require(fields, "connection_state")};
}

AarRequestPayload parse_aar_request(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "aar_request");
    return {parse_envelope(fields), parse_u64(require(fields, "replay_cursor_index")),
            require_enum_string(fields, "control", is_valid_aar_control)};
}

ViewerHeartbeatPayload parse_viewer_heartbeat(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "viewer_heartbeat");
    return {parse_envelope(fields), parse_u64(require(fields, "heartbeat_id"))};
}

}  // namespace icss::protocol
