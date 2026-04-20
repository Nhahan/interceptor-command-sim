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

std::string require_or(const FieldMap& fields, std::string_view key, std::string fallback) {
    const auto it = fields.find(std::string(key));
    if (it == fields.end()) {
        return fallback;
    }
    return it->second;
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

std::string as_string(std::int64_t value) {
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

std::int64_t parse_i64(std::string_view text) {
    std::int64_t value {};
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {}) {
        throw std::runtime_error("invalid int64 field");
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
        parse_u64(require(fields, "capture_wall_time_ms")),
    };
}

TelemetrySample parse_telemetry_sample(const FieldMap& fields) {
    std::string tick_interval_text;
    if (const auto it = fields.find("tick_interval_ms"); it != fields.end()) {
        tick_interval_text = it->second;
    } else {
        tick_interval_text = require(fields, "latency_ms");
    }
    return {
        parse_u64(require(fields, "tick")),
        parse_u32(tick_interval_text),
        parse_float(require(fields, "packet_loss_pct")),
        parse_u64(require(fields, "last_snapshot_timestamp_ms")),
        parse_u64(require(fields, "last_snapshot_wall_time_ms")),
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

bool is_valid_interceptor_status(std::string_view value) {
    return value == "idle" || value == "ready" || value == "intercepting" || value == "complete";
}

bool is_valid_session_phase(std::string_view value) {
    return value == "standby"
        || value == "detecting"
        || value == "tracking"
        || value == "interceptor_ready"
        || value == "engage_ordered"
        || value == "intercepting"
        || value == "assessed"
        || value == "archived";
}

bool is_valid_engage_order_status(std::string_view value) {
    return value == "none" || value == "accepted" || value == "executing" || value == "completed" || value == "rejected";
}

bool is_valid_assessment_code(std::string_view value) {
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
        {"world_width", as_string(static_cast<std::int64_t>(payload.world_width))},
        {"world_height", as_string(static_cast<std::int64_t>(payload.world_height))},
        {"target_start_x", as_string(static_cast<std::int64_t>(payload.target_start_x))},
        {"target_start_y", as_string(static_cast<std::int64_t>(payload.target_start_y))},
        {"target_velocity_x", as_string(static_cast<std::int64_t>(payload.target_velocity_x))},
        {"target_velocity_y", as_string(static_cast<std::int64_t>(payload.target_velocity_y))},
        {"interceptor_start_x", as_string(static_cast<std::int64_t>(payload.interceptor_start_x))},
        {"interceptor_start_y", as_string(static_cast<std::int64_t>(payload.interceptor_start_y))},
        {"interceptor_speed_per_tick", as_string(static_cast<std::int64_t>(payload.interceptor_speed_per_tick))},
        {"intercept_radius", as_string(static_cast<std::int64_t>(payload.intercept_radius))},
        {"engagement_timeout_ticks", as_string(static_cast<std::int64_t>(payload.engagement_timeout_ticks))},
        {"seeker_fov_deg", as_string(static_cast<std::int64_t>(payload.seeker_fov_deg))},
        {"launch_angle_deg", as_string(static_cast<std::int64_t>(payload.launch_angle_deg))},
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

std::string serialize(const ScenarioResetPayload& payload) {
    return join_fields({
        {"kind", "scenario_reset"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"reason", payload.reason},
    });
}

std::string serialize(const TrackAcquirePayload& payload) {
    return join_fields({
        {"kind", "track_acquire"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"target_id", payload.target_id},
    });
}

std::string serialize(const TrackDropPayload& payload) {
    return join_fields({
        {"kind", "track_drop"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"target_id", payload.target_id},
    });
}

std::string serialize(const InterceptorReadyPayload& payload) {
    return join_fields({
        {"kind", "interceptor_ready"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"interceptor_id", payload.interceptor_id},
    });
}

std::string serialize(const EngageOrderPayload& payload) {
    return join_fields({
        {"kind", "engage_order"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"interceptor_id", payload.interceptor_id},
        {"target_id", payload.target_id},
    });
}

std::string serialize(const AssessmentPayload& payload) {
    return join_fields({
        {"kind", "assessment"},
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
        {"assessment_code", payload.assessment_code},
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
        {"capture_wall_time_ms", as_string(payload.header.capture_wall_time_ms)},
        {"phase", payload.phase},
        {"world_width", as_string(static_cast<std::int64_t>(payload.world_width))},
        {"world_height", as_string(static_cast<std::int64_t>(payload.world_height))},
        {"target_id", payload.target_id},
        {"target_active", as_string(payload.target_active)},
        {"target_x", as_string(static_cast<std::int64_t>(payload.target_x))},
        {"target_y", as_string(static_cast<std::int64_t>(payload.target_y))},
        {"target_world_x", as_string(payload.target_world_x)},
        {"target_world_y", as_string(payload.target_world_y)},
        {"target_velocity_x", as_string(static_cast<std::int64_t>(payload.target_velocity_x))},
        {"target_velocity_y", as_string(static_cast<std::int64_t>(payload.target_velocity_y))},
        {"target_velocity_world_x", as_string(payload.target_velocity_world_x)},
        {"target_velocity_world_y", as_string(payload.target_velocity_world_y)},
        {"target_heading_deg", as_string(payload.target_heading_deg)},
        {"interceptor_id", payload.interceptor_id},
        {"interceptor_active", as_string(payload.interceptor_active)},
        {"interceptor_x", as_string(static_cast<std::int64_t>(payload.interceptor_x))},
        {"interceptor_y", as_string(static_cast<std::int64_t>(payload.interceptor_y))},
        {"interceptor_world_x", as_string(payload.interceptor_world_x)},
        {"interceptor_world_y", as_string(payload.interceptor_world_y)},
        {"interceptor_velocity_world_x", as_string(payload.interceptor_velocity_world_x)},
        {"interceptor_velocity_world_y", as_string(payload.interceptor_velocity_world_y)},
        {"interceptor_heading_deg", as_string(payload.interceptor_heading_deg)},
        {"interceptor_speed_per_tick", as_string(static_cast<std::int64_t>(payload.interceptor_speed_per_tick))},
        {"interceptor_acceleration_per_tick", as_string(payload.interceptor_acceleration_per_tick)},
        {"intercept_radius", as_string(static_cast<std::int64_t>(payload.intercept_radius))},
        {"engagement_timeout_ticks", as_string(static_cast<std::int64_t>(payload.engagement_timeout_ticks))},
        {"seeker_fov_deg", as_string(payload.seeker_fov_deg)},
        {"seeker_lock", as_string(payload.seeker_lock)},
        {"off_boresight_deg", as_string(payload.off_boresight_deg)},
        {"predicted_intercept_valid", as_string(payload.predicted_intercept_valid)},
        {"predicted_intercept_x", as_string(payload.predicted_intercept_x)},
        {"predicted_intercept_y", as_string(payload.predicted_intercept_y)},
        {"time_to_intercept_s", as_string(payload.time_to_intercept_s)},
        {"track_active", as_string(payload.track_active)},
        {"track_estimated_x", as_string(payload.track_estimated_x)},
        {"track_estimated_y", as_string(payload.track_estimated_y)},
        {"track_estimated_vx", as_string(payload.track_estimated_vx)},
        {"track_estimated_vy", as_string(payload.track_estimated_vy)},
        {"track_measurement_valid", as_string(payload.track_measurement_valid)},
        {"track_measurement_x", as_string(payload.track_measurement_x)},
        {"track_measurement_y", as_string(payload.track_measurement_y)},
        {"track_measurement_residual_distance", as_string(payload.track_measurement_residual_distance)},
        {"track_covariance_trace", as_string(payload.track_covariance_trace)},
        {"track_measurement_age_ticks", as_string(static_cast<std::int64_t>(payload.track_measurement_age_ticks))},
        {"track_missed_updates", as_string(static_cast<std::int64_t>(payload.track_missed_updates))},
        {"interceptor_status", payload.interceptor_status},
        {"engage_order_status", payload.engage_order_status},
        {"assessment_ready", as_string(payload.assessment_ready)},
        {"assessment_code", payload.assessment_code},
        {"launch_angle_deg", as_string(payload.launch_angle_deg)},
    });
}

std::string serialize(const TelemetryPayload& payload) {
    return join_fields({
        {"kind", "telemetry"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"tick", as_string(payload.sample.tick)},
        {"tick_interval_ms", as_string(payload.sample.tick_interval_ms)},
        {"packet_loss_pct", as_string(payload.sample.packet_loss_pct)},
        {"last_snapshot_timestamp_ms", as_string(payload.sample.last_snapshot_timestamp_ms)},
        {"last_snapshot_wall_time_ms", as_string(payload.sample.last_snapshot_wall_time_ms)},
        {"connection_state", payload.connection_state},
        {"event_tick", as_string(payload.event_tick)},
        {"event_type", payload.event_type},
        {"event_summary", payload.event_summary},
    });
}

std::string serialize(const DisplayHeartbeatPayload& payload) {
    return join_fields({
        {"kind", "display_heartbeat"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"heartbeat_id", as_string(payload.heartbeat_id)},
        {"client_send_wall_time_ms", as_string(payload.client_send_wall_time_ms)},
    });
}

std::string serialize(const DisplayHeartbeatAckPayload& payload) {
    return join_fields({
        {"kind", "display_heartbeat_ack"},
        {"session_id", as_string(payload.envelope.session_id)},
        {"sender_id", as_string(payload.envelope.sender_id)},
        {"sequence", as_string(payload.envelope.sequence)},
        {"heartbeat_id", as_string(payload.heartbeat_id)},
        {"client_send_wall_time_ms", as_string(payload.client_send_wall_time_ms)},
        {"server_receive_wall_time_ms", as_string(payload.server_receive_wall_time_ms)},
        {"server_send_wall_time_ms", as_string(payload.server_send_wall_time_ms)},
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
    return {
        parse_envelope(fields),
        require(fields, "scenario_name"),
        static_cast<int>(parse_i64(require_or(fields, "world_width", "2304"))),
        static_cast<int>(parse_i64(require_or(fields, "world_height", "1536"))),
        static_cast<int>(parse_i64(require_or(fields, "target_start_x", "480"))),
        static_cast<int>(parse_i64(require_or(fields, "target_start_y", "1200"))),
        static_cast<int>(parse_i64(require_or(fields, "target_velocity_x", "5"))),
        static_cast<int>(parse_i64(require_or(fields, "target_velocity_y", "-3"))),
        static_cast<int>(parse_i64(require_or(fields, "interceptor_start_x", "0"))),
        static_cast<int>(parse_i64(require_or(fields, "interceptor_start_y", "0"))),
        static_cast<int>(parse_i64(require_or(fields, "interceptor_speed_per_tick", "32"))),
        static_cast<int>(parse_i64(require_or(fields, "intercept_radius", "24"))),
        static_cast<int>(parse_i64(require_or(fields, "engagement_timeout_ticks", "60"))),
        static_cast<int>(parse_i64(require_or(fields, "seeker_fov_deg", "45"))),
        static_cast<int>(parse_i64(require_or(fields, "launch_angle_deg", "45"))),
    };
}

ScenarioStopPayload parse_scenario_stop(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "scenario_stop");
    return {parse_envelope(fields), require(fields, "reason")};
}

ScenarioResetPayload parse_scenario_reset(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "scenario_reset");
    return {parse_envelope(fields), require(fields, "reason")};
}

TrackAcquirePayload parse_track_acquire(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "track_acquire");
    return {parse_envelope(fields), require(fields, "target_id")};
}

TrackDropPayload parse_track_drop(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "track_drop");
    return {parse_envelope(fields), require(fields, "target_id")};
}

InterceptorReadyPayload parse_interceptor_ready(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "interceptor_ready");
    return {parse_envelope(fields), require(fields, "interceptor_id")};
}

EngageOrderPayload parse_engage_order(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "engage_order");
    return {parse_envelope(fields), require(fields, "interceptor_id"), require(fields, "target_id")};
}

AssessmentPayload parse_assessment(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "assessment");
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
            require_enum_string(fields, "assessment_code", is_valid_assessment_code),
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
        require_enum_string(fields, "phase", is_valid_session_phase),
        static_cast<int>(parse_i64(require_or(fields, "world_width", "2304"))),
        static_cast<int>(parse_i64(require_or(fields, "world_height", "1536"))),
        require(fields, "target_id"),
        parse_bool(require(fields, "target_active")),
        static_cast<int>(parse_i64(require(fields, "target_x"))),
        static_cast<int>(parse_i64(require(fields, "target_y"))),
        parse_float(require_or(fields, "target_world_x", "0")),
        parse_float(require_or(fields, "target_world_y", "0")),
        static_cast<int>(parse_i64(require_or(fields, "target_velocity_x", "0"))),
        static_cast<int>(parse_i64(require_or(fields, "target_velocity_y", "0"))),
        parse_float(require_or(fields, "target_velocity_world_x", "0")),
        parse_float(require_or(fields, "target_velocity_world_y", "0")),
        parse_float(require_or(fields, "target_heading_deg", "0")),
        require(fields, "interceptor_id"),
        parse_bool(require(fields, "interceptor_active")),
        static_cast<int>(parse_i64(require(fields, "interceptor_x"))),
        static_cast<int>(parse_i64(require(fields, "interceptor_y"))),
        parse_float(require_or(fields, "interceptor_world_x", "0")),
        parse_float(require_or(fields, "interceptor_world_y", "0")),
        parse_float(require_or(fields, "interceptor_velocity_world_x", "0")),
        parse_float(require_or(fields, "interceptor_velocity_world_y", "0")),
        parse_float(require_or(fields, "interceptor_heading_deg", "0")),
        static_cast<int>(parse_i64(require_or(fields, "interceptor_speed_per_tick", "0"))),
        parse_float(require_or(fields, "interceptor_acceleration_per_tick", "0")),
        static_cast<int>(parse_i64(require_or(fields, "intercept_radius", "0"))),
        static_cast<int>(parse_i64(require_or(fields, "engagement_timeout_ticks", "0"))),
        parse_float(require_or(fields, "seeker_fov_deg", "0")),
        parse_bool(require_or(fields, "seeker_lock", "false")),
        parse_float(require_or(fields, "off_boresight_deg", "0")),
        parse_bool(require_or(fields, "predicted_intercept_valid", "false")),
        parse_float(require_or(fields, "predicted_intercept_x", "0")),
        parse_float(require_or(fields, "predicted_intercept_y", "0")),
        parse_float(require_or(fields, "time_to_intercept_s", "0")),
        parse_bool(require(fields, "track_active")),
        parse_float(require_or(fields, "track_estimated_x", "0")),
        parse_float(require_or(fields, "track_estimated_y", "0")),
        parse_float(require_or(fields, "track_estimated_vx", "0")),
        parse_float(require_or(fields, "track_estimated_vy", "0")),
        parse_bool(require_or(fields, "track_measurement_valid", "false")),
        parse_float(require_or(fields, "track_measurement_x", "0")),
        parse_float(require_or(fields, "track_measurement_y", "0")),
        parse_float(require_or(fields, "track_measurement_residual_distance", "0")),
        parse_float(require_or(fields, "track_covariance_trace", "0")),
        static_cast<int>(parse_i64(require_or(fields, "track_measurement_age_ticks", "0"))),
        static_cast<int>(parse_i64(require_or(fields, "track_missed_updates", "0"))),
        require_enum_string(fields, "interceptor_status", is_valid_interceptor_status),
        require_enum_string(fields, "engage_order_status", is_valid_engage_order_status),
        parse_bool(require(fields, "assessment_ready")),
        require_enum_string(fields, "assessment_code", is_valid_assessment_code),
        parse_float(require_or(fields, "launch_angle_deg", "45")),
    };
}

TelemetryPayload parse_telemetry(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "telemetry");
    return {
        parse_envelope(fields),
        parse_telemetry_sample(fields),
        require(fields, "connection_state"),
        parse_u64(require(fields, "event_tick")),
        require(fields, "event_type"),
        require(fields, "event_summary"),
    };
}

AarRequestPayload parse_aar_request(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "aar_request");
    return {parse_envelope(fields), parse_u64(require(fields, "replay_cursor_index")),
            require_enum_string(fields, "control", is_valid_aar_control)};
}

DisplayHeartbeatPayload parse_display_heartbeat(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "display_heartbeat");
    return {
        parse_envelope(fields),
        parse_u64(require(fields, "heartbeat_id")),
        parse_u64(require(fields, "client_send_wall_time_ms")),
    };
}

DisplayHeartbeatAckPayload parse_display_heartbeat_ack(std::string_view wire) {
    const auto fields = parse_fields(wire);
    require_kind(fields, "display_heartbeat_ack");
    return {
        parse_envelope(fields),
        parse_u64(require(fields, "heartbeat_id")),
        parse_u64(require(fields, "client_send_wall_time_ms")),
        parse_u64(require(fields, "server_receive_wall_time_ms")),
        parse_u64(require(fields, "server_send_wall_time_ms")),
    };
}

}  // namespace icss::protocol
