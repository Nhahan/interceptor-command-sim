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
        fields.emplace(std::string(part.substr(0, pos)), std::string(part.substr(pos + 1)));
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
        wire += value;
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

}  // namespace

std::string serialize(const SessionCreatePayload& payload) {
    return join_fields({
        {"kind", "session_create"},
        {"requested_sender_id", as_string(payload.requested_sender_id)},
        {"scenario_name", payload.scenario_name},
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
        {"asset_ready", as_string(payload.asset_ready)},
        {"judgment_ready", as_string(payload.judgment_ready)},
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

std::string serialize(const AarRequestPayload& payload) {
    return join_fields({
        {"kind", "aar_request"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"replay_cursor_index", as_string(payload.replay_cursor_index)},
    });
}

SessionCreatePayload parse_session_create(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "session_create");
    return {parse_u32(require(fields, "requested_sender_id")), require(fields, "scenario_name")};
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

SnapshotPayload parse_snapshot(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "world_snapshot");
    return {
        parse_envelope(fields),
        parse_snapshot_header(fields),
        require(fields, "target_id"),
        require(fields, "asset_id"),
        parse_bool(require(fields, "tracking_active")),
        parse_bool(require(fields, "asset_ready")),
        parse_bool(require(fields, "judgment_ready")),
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
    return {parse_envelope(fields), parse_u64(require(fields, "replay_cursor_index"))};
}

}  // namespace icss::protocol
