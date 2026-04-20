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

    const SessionJoinPayload session_join {{1001U, 101U, 1U}, "fire_control_console"};
    const auto join_wire = serialize(session_join);
    const auto parsed_join = parse_session_join(join_wire);
    assert(parsed_join.client_role == "fire_control_console");

    const SessionLeavePayload leave_payload {{1001U, 101U, 2U}, "operator requested leave"};
    const auto leave_wire = serialize(leave_payload);
    const auto parsed_leave = parse_session_leave(leave_wire);
    assert(parsed_leave.reason == "operator requested leave");

    const ScenarioStartPayload start_payload {{1001U, 101U, 3U}, "basic_intercept_training", 576, 384, 80, 300, 5, -3, 0, 0, 32, 24, 60, 45, 37};
    const auto start_wire = serialize(start_payload);
    const auto parsed_start = parse_scenario_start(start_wire);
    assert(parsed_start.scenario_name == "basic_intercept_training");
    assert(parsed_start.world_width == 576);
    assert(parsed_start.target_velocity_y == -3);
    assert(parsed_start.interceptor_speed_per_tick == 32);
    assert(parsed_start.seeker_fov_deg == 45);
    assert(parsed_start.launch_angle_deg == 37);

    const ScenarioStopPayload stop_payload {{1001U, 101U, 4U}, "scenario stop requested"};
    const auto stop_wire = serialize(stop_payload);
    const auto parsed_stop = parse_scenario_stop(stop_wire);
    assert(parsed_stop.reason == "scenario stop requested");

    const ScenarioResetPayload reset_payload {{1001U, 101U, 5U}, "reset for new run"};
    const auto reset_wire = serialize(reset_payload);
    const auto parsed_reset = parse_scenario_reset(reset_wire);
    assert(parsed_reset.reason == "reset for new run");

    const EngageOrderPayload command {
        {1001U, 101U, 7U},
        "interceptor-alpha",
        "target-alpha",
    };
    const auto command_wire = serialize(command);
    const auto parsed_command = parse_engage_order(command_wire);
    assert(parsed_command.envelope.session_id == 1001U);
    assert(parsed_command.envelope.sender_id == 101U);
    assert(parsed_command.target_id == "target-alpha");
    assert(parsed_command.interceptor_id == "interceptor-alpha");

    const TrackDropPayload track_drop {
        {1001U, 101U, 8U},
        "target-alpha",
    };
    const auto track_drop_wire = serialize(track_drop);
    const auto parsed_track_drop = parse_track_drop(track_drop_wire);
    assert(parsed_track_drop.envelope.sequence == 8U);
    assert(parsed_track_drop.target_id == "target-alpha");

    const TelemetryPayload telemetry {
        {1001U, 1U, 8U},
        {3U, 35U, 12.5F, 1'776'327'000'800ULL},
        "reconnected",
        3U,
        "assessment_produced",
        "Engagement assessed",
    };
    const auto telemetry_wire = serialize(telemetry);
    const auto parsed_telemetry = parse_telemetry(telemetry_wire);
    assert(parsed_telemetry.sample.tick == 3U);
    assert(parsed_telemetry.sample.tick_interval_ms == 35U);
    assert(parsed_telemetry.sample.packet_loss_pct > 12.0F);
    assert(parsed_telemetry.connection_state == "reconnected");
    assert(parsed_telemetry.event_tick == 3U);
    assert(parsed_telemetry.event_type == "assessment_produced");
    assert(parsed_telemetry.event_summary == "Engagement assessed");

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
        4.25F,
        6.75F,
        5,
        -3,
        5.0F,
        -3.0F,
        -31.0F,
        "interceptor-alpha",
        true,
        8,
        2,
        8.50F,
        2.25F,
        10.25F,
        1.50F,
        8.3F,
        32,
        6.0F,
        24,
        60,
        45.0F,
        true,
        9.0F,
        true,
        18.0F,
        12.5F,
        1.8F,
        true,
        4.5F,
        6.5F,
        4.8F,
        -2.7F,
        true,
        4.7F,
        6.4F,
        2.6F,
        18.5F,
        0,
        1,
        "ready",
        "accepted",
        false,
        "pending",
        37.0F,
    };
    const auto snapshot_wire = serialize(snapshot);
    const auto parsed_snapshot = parse_snapshot(snapshot_wire);
    assert(parsed_snapshot.phase == "tracking");
    assert(parsed_snapshot.world_width == 576);
    assert(parsed_snapshot.target_world_x > 4.0F);
    assert(parsed_snapshot.interceptor_world_x > 8.0F);
    assert(parsed_snapshot.target_velocity_x == 5);
    assert(parsed_snapshot.target_velocity_world_x == 5.0F);
    assert(parsed_snapshot.target_heading_deg < 0.0F);
    assert(parsed_snapshot.target_active);
    assert(parsed_snapshot.target_x == 4);
    assert(parsed_snapshot.target_y == 6);
    assert(parsed_snapshot.interceptor_active);
    assert(parsed_snapshot.interceptor_x == 8);
    assert(parsed_snapshot.interceptor_y == 2);
    assert(parsed_snapshot.interceptor_speed_per_tick == 32);
    assert(parsed_snapshot.seeker_fov_deg == 45.0F);
    assert(parsed_snapshot.seeker_lock);
    assert(parsed_snapshot.off_boresight_deg == 9.0F);
    assert(parsed_snapshot.predicted_intercept_valid);
    assert(parsed_snapshot.time_to_intercept_s > 1.0F);
    assert(parsed_snapshot.track_estimated_x > 4.0F);
    assert(parsed_snapshot.track_measurement_valid);
    assert(parsed_snapshot.track_measurement_residual_distance > 2.0F);
    assert(parsed_snapshot.track_covariance_trace > 10.0F);
    assert(parsed_snapshot.interceptor_status == "ready");
    assert(parsed_snapshot.engage_order_status == "accepted");
    assert(parsed_snapshot.assessment_code == "pending");
    assert(parsed_snapshot.launch_angle_deg == 37.0F);

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
        "assessment_produced",
        "authoritative intercept complete",
    };
    const auto aar_response_wire = serialize(aar_response);
    const auto parsed_aar_response = parse_aar_response(aar_response_wire);
    assert(parsed_aar_response.replay_cursor_index == 8U);
    assert(parsed_aar_response.control == "absolute");
    assert(parsed_aar_response.requested_index == 11U);
    assert(parsed_aar_response.clamped);
    assert(parsed_aar_response.assessment_code == "intercept_success");
    assert(parsed_aar_response.total_events == 12U);
    assert(parsed_aar_response.event_type == "assessment_produced");

    const AarRequestPayload aar_request {
        {1001U, 101U, 13U},
        7U,
        "step_backward",
    };
    const auto aar_request_wire = serialize(aar_request);
    const auto parsed_aar_request = parse_aar_request(aar_request_wire);
    assert(parsed_aar_request.replay_cursor_index == 7U);
    assert(parsed_aar_request.control == "step_backward");

    const DisplayHeartbeatPayload heartbeat {
        {1001U, 201U, 12U},
        42U,
        123456U,
    };
    const auto heartbeat_wire = serialize(heartbeat);
    const auto parsed_heartbeat = parse_display_heartbeat(heartbeat_wire);
    assert(parsed_heartbeat.heartbeat_id == 42U);
    assert(parsed_heartbeat.client_send_wall_time_ms == 123456U);

    const DisplayHeartbeatAckPayload heartbeat_ack {
        {1001U, 1U, 14U},
        42U,
        123456U,
        123460U,
        123461U,
    };
    const auto heartbeat_ack_wire = serialize(heartbeat_ack);
    const auto parsed_heartbeat_ack = parse_display_heartbeat_ack(heartbeat_ack_wire);
    assert(parsed_heartbeat_ack.heartbeat_id == 42U);
    assert(parsed_heartbeat_ack.client_send_wall_time_ms == 123456U);
    assert(parsed_heartbeat_ack.server_receive_wall_time_ms == 123460U);
    assert(parsed_heartbeat_ack.server_send_wall_time_ms == 123461U);

    bool rejected_wrong_kind = false;
    try {
        static_cast<void>(parse_engage_order(session_wire));
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
