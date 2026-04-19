#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "icss/protocol/serialization.hpp"

namespace icss::viewer_gui {

#if !defined(_WIN32)
void perform_control_action(std::string_view action_label,
                            ViewerState& state,
                            const ViewerOptions& options,
                            FrameMode frame_mode,
                            TcpSocket& control_socket) {
    if (apply_parameter_action(state, action_label)) {
        return;
    }

    ensure_control_connected(control_socket, state, options, frame_mode);
    const auto clear_local_track = [&]() {
        state.snapshot.track = {};
    };
    const auto send_and_expect_ack = [&](std::string_view kind,
                                         std::string payload,
                                         std::string_view display_label = {}) -> icss::protocol::CommandAckPayload {
        const auto effective_label = display_label.empty() ? action_label : display_label;
        send_frame(control_socket, frame_mode, kind, payload);
        const auto frame = recv_frame(control_socket, frame_mode);
        if (frame.kind != "command_ack") {
            throw std::runtime_error("expected command_ack after " + std::string(kind));
        }
        const auto ack = icss::protocol::parse_command_ack(frame.payload);
        state.control.last_ok = ack.accepted;
        state.control.last_label = std::string(effective_label);
        state.control.last_message = ack.reason;
        push_timeline_entry(state, control_timeline_message(effective_label, ack.accepted, ack.reason));
        if (!ack.accepted) {
            return ack;
        }
        if (effective_label != "Review") {
            state.review.visible = false;
        }
        if (effective_label == "Start") {
            state.snapshot.phase = icss::core::SessionPhase::Detecting;
            state.snapshot.target.active = true;
            state.snapshot.command_status = icss::core::CommandLifecycle::None;
            state.snapshot.asset_status = icss::core::AssetStatus::Idle;
            state.snapshot.asset_velocity = {0.0F, 0.0F};
            state.snapshot.asset_heading_deg = 0.0F;
            state.effective_guidance_active = false;
            state.target_history.clear();
            state.asset_history.clear();
        } else if (effective_label == "Activate") {
            state.snapshot.phase = icss::core::SessionPhase::AssetReady;
            state.snapshot.asset.active = true;
            state.snapshot.asset_status = icss::core::AssetStatus::Ready;
        } else if (effective_label == "Guidance On") {
            if (state.snapshot.phase == icss::core::SessionPhase::Detecting) {
                state.snapshot.phase = icss::core::SessionPhase::Tracking;
            }
            state.snapshot.track.active = true;
            state.effective_guidance_active = true;
        } else if (effective_label == "Guidance Off") {
            if (state.snapshot.phase == icss::core::SessionPhase::Tracking) {
                state.snapshot.phase = icss::core::SessionPhase::Detecting;
            }
            clear_local_track();
            state.effective_guidance_active = false;
        } else if (effective_label == "Command") {
            state.snapshot.phase = icss::core::SessionPhase::CommandIssued;
            state.snapshot.command_status = icss::core::CommandLifecycle::Accepted;
            state.snapshot.asset_status = icss::core::AssetStatus::Ready;
            const auto launch_angle_rad = state.snapshot.launch_angle_deg * 3.1415926535F / 180.0F;
            const auto launch_speed = static_cast<float>(state.snapshot.interceptor_speed_per_tick);
            state.snapshot.asset_velocity = {
                std::cos(launch_angle_rad) * launch_speed,
                std::sin(launch_angle_rad) * launch_speed,
            };
            state.snapshot.asset_heading_deg = state.snapshot.launch_angle_deg;
        } else if (effective_label == "Reset") {
            sync_preview_from_planned_scenario(state);
            state.snapshot.phase = icss::core::SessionPhase::Initialized;
            state.snapshot.target.active = false;
            state.snapshot.asset.active = false;
            clear_local_track();
            state.snapshot.command_status = icss::core::CommandLifecycle::None;
            state.snapshot.asset_status = icss::core::AssetStatus::Idle;
            state.snapshot.judgment = {};
            state.effective_guidance_active = false;
            state.review = {};
        }
        return ack;
    };

    if (action_label == "Start") {
        const auto actual_start = randomize_start_scenario(state.planned_scenario, state.control.start_seed++);
        const auto ack = send_and_expect_ack(
            "scenario_start",
            icss::protocol::serialize(icss::protocol::ScenarioStartPayload{
                {options.session_id, options.sender_id, state.control.sequence++},
                actual_start.name,
                actual_start.world_width,
                actual_start.world_height,
                actual_start.target_start_x,
                actual_start.target_start_y,
                actual_start.target_velocity_x,
                actual_start.target_velocity_y,
                actual_start.interceptor_start_x,
                actual_start.interceptor_start_y,
                actual_start.interceptor_speed_per_tick,
                actual_start.intercept_radius,
                actual_start.engagement_timeout_ticks,
                actual_start.seeker_fov_deg,
                actual_start.launch_angle_deg}));
        if (ack.accepted) {
            sync_preview_from_scenario(state, actual_start);
            push_timeline_entry(
                state,
                "[start randomized] target=(" + std::to_string(actual_start.target_start_x) + "," + std::to_string(actual_start.target_start_y)
                    + ") velocity=(" + std::to_string(actual_start.target_velocity_x) + "," + std::to_string(actual_start.target_velocity_y)
                    + ") launch_angle=" + std::to_string(actual_start.launch_angle_deg));
        }
        return;
    }
    if (action_label == "Guidance") {
        const bool turning_on = !state.snapshot.track.active;
        const auto display_label = turning_on ? "Guidance On" : "Guidance Off";
        if (turning_on) {
            send_and_expect_ack("track_request",
                                icss::protocol::serialize(icss::protocol::TrackRequestPayload{{options.session_id, options.sender_id, state.control.sequence++}, "target-alpha"}),
                                display_label);
        } else {
            send_and_expect_ack("track_release",
                                icss::protocol::serialize(icss::protocol::TrackReleasePayload{{options.session_id, options.sender_id, state.control.sequence++}, "target-alpha"}),
                                display_label);
        }
        return;
    }
    if (action_label == "Activate") {
        send_and_expect_ack("asset_activate", icss::protocol::serialize(icss::protocol::AssetActivatePayload{{options.session_id, options.sender_id, state.control.sequence++}, "asset-interceptor"}));
        return;
    }
    if (action_label == "Command") {
        send_and_expect_ack("command_issue", icss::protocol::serialize(icss::protocol::CommandIssuePayload{{options.session_id, options.sender_id, state.control.sequence++}, "asset-interceptor", "target-alpha"}));
        return;
    }
    if (action_label == "Reset") {
        send_and_expect_ack("scenario_reset", icss::protocol::serialize(icss::protocol::ScenarioResetPayload{{options.session_id, options.sender_id, state.control.sequence++}, "gui control panel reset"}));
        state.recent_server_events.clear();
        state.target_history.clear();
        state.asset_history.clear();
        state.last_server_event_tick = 0;
        state.last_server_event_type = "none";
        state.last_server_event_summary = "no server event";
        state.timeline_scroll_lines = 0;
        state.review.visible = false;
        return;
    }
    if (action_label == "Review") {
        if (!review_available(state)) {
            state.control.last_ok = false;
            state.control.last_label = "Review";
            state.control.last_message = "review available after judgment or archive";
            push_timeline_entry(state, control_timeline_message("Review", false, state.control.last_message));
            return;
        }
        send_frame(control_socket,
                   frame_mode,
                   "aar_request",
                   icss::protocol::serialize(icss::protocol::AarRequestPayload{{options.session_id, options.sender_id, state.control.sequence++}, 999U, "absolute"}));
        const auto frame = recv_frame(control_socket, frame_mode);
        if (frame.kind != "aar_response") {
            throw std::runtime_error("expected aar_response after Review action");
        }
        const auto response = icss::protocol::parse_aar_response(frame.payload);
        state.control.last_ok = true;
        state.control.last_label = "Review";
        state.control.last_message = "server-side review loaded";
        push_timeline_entry(state, control_timeline_message("Review", true, state.control.last_message));
        state.review.available = true;
        state.review.loaded = true;
        state.review.visible = true;
        state.timeline_scroll_lines = 0;
        state.review.cursor_index = response.replay_cursor_index;
        state.review.total_events = response.total_events;
        state.review.judgment_code = response.judgment_code;
        state.review.resilience_case = response.resilience_case;
        state.review.event_type = response.event_type;
        state.review.event_summary = response.event_summary;
    }
}
#endif

}  // namespace icss::viewer_gui
