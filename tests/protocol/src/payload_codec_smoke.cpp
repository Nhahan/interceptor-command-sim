#include <cassert>
#include <stdexcept>
#include <string>

#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"

int main() {
    using namespace icss::protocol;

    const SessionCreatePayload session_create {101U, "basic_intercept_training"};
    const auto session_wire = serialize(session_create);
    const auto parsed_session = parse_session_create(session_wire);
    assert(parsed_session.requested_sender_id == 101U);
    assert(parsed_session.scenario_name == "basic_intercept_training");

    const SessionJoinPayload session_join {{1001U, 101U, 1U}, "command_console"};
    const auto join_wire = serialize(session_join);
    const auto parsed_join = parse_session_join(join_wire);
    assert(parsed_join.client_role == "command_console");

    const SessionLeavePayload leave_payload {{1001U, 101U, 2U}, "operator requested leave"};
    const auto leave_wire = serialize(leave_payload);
    const auto parsed_leave = parse_session_leave(leave_wire);
    assert(parsed_leave.reason == "operator requested leave");

    const ScenarioStartPayload start_payload {{1001U, 101U, 3U}, "basic_intercept_training", 576, 384, 80, 300, 5, -3, 160, 60, 14, 12, 26};
    const auto start_wire = serialize(start_payload);
    const auto parsed_start = parse_scenario_start(start_wire);
    assert(parsed_start.scenario_name == "basic_intercept_training");
    assert(parsed_start.world_width == 576);
    assert(parsed_start.target_velocity_y == -3);
    assert(parsed_start.interceptor_speed_per_tick == 14);

    const ScenarioStopPayload stop_payload {{1001U, 101U, 4U}, "scenario stop requested"};
    const auto stop_wire = serialize(stop_payload);
    const auto parsed_stop = parse_scenario_stop(stop_wire);
    assert(parsed_stop.reason == "scenario stop requested");

    const ScenarioResetPayload reset_payload {{1001U, 101U, 5U}, "reset for new run"};
    const auto reset_wire = serialize(reset_payload);
    const auto parsed_reset = parse_scenario_reset(reset_wire);
    assert(parsed_reset.reason == "reset for new run");

    const CommandIssuePayload command {
        {1001U, 101U, 7U},
        "asset-interceptor",
        "target-alpha",
    };
    const auto command_wire = serialize(command);
    const auto parsed_command = parse_command_issue(command_wire);
    assert(parsed_command.envelope.session_id == 1001U);
    assert(parsed_command.envelope.sender_id == 101U);
    assert(parsed_command.target_id == "target-alpha");
    assert(parsed_command.asset_id == "asset-interceptor");

    const TelemetryPayload telemetry {
        {1001U, 1U, 8U},
        {3U, 35U, 12.5F, 1'776'327'000'800ULL},
        "reconnected",
        3U,
        "judgment_produced",
        "Judgment produced",
    };
    const auto telemetry_wire = serialize(telemetry);
    const auto parsed_telemetry = parse_telemetry(telemetry_wire);
    assert(parsed_telemetry.sample.tick == 3U);
    assert(parsed_telemetry.sample.latency_ms == 35U);
    assert(parsed_telemetry.sample.packet_loss_pct > 12.0F);
    assert(parsed_telemetry.connection_state == "reconnected");
    assert(parsed_telemetry.event_tick == 3U);
    assert(parsed_telemetry.event_type == "judgment_produced");
    assert(parsed_telemetry.event_summary == "Judgment produced");

    const SnapshotPayload snapshot {
        {1001U, 1U, 9U},
        {3U, 1'776'327'000'900ULL, 9U},
        "tracking",
        576,
        384,
        "target-alpha",
        true,
        4,
        6,
        5,
        -3,
        "asset-interceptor",
        true,
        8,
        2,
        14,
        12,
        26,
        true,
        82,
        "ready",
        "accepted",
        false,
        "pending",
    };
    const auto snapshot_wire = serialize(snapshot);
    const auto parsed_snapshot = parse_snapshot(snapshot_wire);
    assert(parsed_snapshot.phase == "tracking");
    assert(parsed_snapshot.world_width == 576);
    assert(parsed_snapshot.target_velocity_x == 5);
    assert(parsed_snapshot.target_active);
    assert(parsed_snapshot.target_x == 4);
    assert(parsed_snapshot.target_y == 6);
    assert(parsed_snapshot.asset_active);
    assert(parsed_snapshot.asset_x == 8);
    assert(parsed_snapshot.asset_y == 2);
    assert(parsed_snapshot.interceptor_speed_per_tick == 14);
    assert(parsed_snapshot.track_confidence_pct == 82);
    assert(parsed_snapshot.asset_status == "ready");
    assert(parsed_snapshot.command_status == "accepted");
    assert(parsed_snapshot.judgment_code == "pending");

    const CommandAckPayload ack {
        {1001U, 1U, 10U},
        true,
        "track accepted",
    };
    const auto ack_wire = serialize(ack);
    const auto parsed_ack = parse_command_ack(ack_wire);
    assert(parsed_ack.accepted);
    assert(parsed_ack.reason == "track accepted");

    const AarResponsePayload aar_response {
        {1001U, 1U, 11U},
        8U,
        "absolute",
        11U,
        true,
        "intercept_success",
        "reconnect_and_resync",
        12U,
        "judgment_produced",
        "authoritative intercept complete",
    };
    const auto aar_response_wire = serialize(aar_response);
    const auto parsed_aar_response = parse_aar_response(aar_response_wire);
    assert(parsed_aar_response.replay_cursor_index == 8U);
    assert(parsed_aar_response.control == "absolute");
    assert(parsed_aar_response.requested_index == 11U);
    assert(parsed_aar_response.clamped);
    assert(parsed_aar_response.judgment_code == "intercept_success");
    assert(parsed_aar_response.total_events == 12U);
    assert(parsed_aar_response.event_type == "judgment_produced");

    const AarRequestPayload aar_request {
        {1001U, 101U, 13U},
        7U,
        "step_backward",
    };
    const auto aar_request_wire = serialize(aar_request);
    const auto parsed_aar_request = parse_aar_request(aar_request_wire);
    assert(parsed_aar_request.replay_cursor_index == 7U);
    assert(parsed_aar_request.control == "step_backward");

    const ViewerHeartbeatPayload heartbeat {
        {1001U, 201U, 12U},
        42U,
    };
    const auto heartbeat_wire = serialize(heartbeat);
    const auto parsed_heartbeat = parse_viewer_heartbeat(heartbeat_wire);
    assert(parsed_heartbeat.heartbeat_id == 42U);

    bool rejected_wrong_kind = false;
    try {
        static_cast<void>(parse_command_issue(session_wire));
    } catch (const std::runtime_error&) {
        rejected_wrong_kind = true;
    }
    assert(rejected_wrong_kind);

    bool rejected_bad_aar_control = false;
    try {
        static_cast<void>(parse_aar_request(
            "kind=aar_request;session_id=1001;sender_id=101;sequence=14;replay_cursor_index=4;control=rewind"));
    } catch (const std::runtime_error&) {
        rejected_bad_aar_control = true;
    }
    assert(rejected_bad_aar_control);

    return 0;
}
