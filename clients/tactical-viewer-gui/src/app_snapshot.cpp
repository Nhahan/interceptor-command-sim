#include "app.hpp"

#include <algorithm>
#include <cmath>

namespace icss::viewer_gui {

void apply_snapshot(ViewerState& state, const icss::protocol::SnapshotPayload& payload) {
    if (state.received_snapshot
        && payload.header.snapshot_sequence <= state.snapshot.header.snapshot_sequence) {
        return;
    }
    state.snapshot.envelope = payload.envelope;
    state.snapshot.header = payload.header;
    state.snapshot.phase = parse_phase(payload.phase);
    state.snapshot.world_width = payload.world_width;
    state.snapshot.world_height = payload.world_height;
    state.snapshot.target.id = payload.target_id;
    state.snapshot.target.active = payload.target_active;
    state.snapshot.target.position = {payload.target_x, payload.target_y};
    state.snapshot.target_world_position = {payload.target_world_x, payload.target_world_y};
    state.snapshot.target_velocity_x = payload.target_velocity_x;
    state.snapshot.target_velocity_y = payload.target_velocity_y;
    state.snapshot.target_velocity = {payload.target_velocity_world_x, payload.target_velocity_world_y};
    state.snapshot.target_heading_deg = payload.target_heading_deg;
    state.snapshot.interceptor.id = payload.interceptor_id;
    state.snapshot.interceptor.active = payload.interceptor_active;
    state.snapshot.interceptor.position = {payload.interceptor_x, payload.interceptor_y};
    state.snapshot.interceptor_world_position = {payload.interceptor_world_x, payload.interceptor_world_y};
    state.snapshot.interceptor_velocity = {payload.interceptor_velocity_world_x, payload.interceptor_velocity_world_y};
    state.snapshot.interceptor_heading_deg = payload.interceptor_heading_deg;
    state.snapshot.interceptor_speed_per_tick = payload.interceptor_speed_per_tick;
    state.snapshot.interceptor_acceleration_per_tick = payload.interceptor_acceleration_per_tick;
    state.snapshot.intercept_radius = payload.intercept_radius;
    state.snapshot.engagement_timeout_ticks = payload.engagement_timeout_ticks;
    state.snapshot.seeker_fov_deg = payload.seeker_fov_deg;
    state.snapshot.seeker_lock = payload.seeker_lock;
    state.snapshot.off_boresight_deg = payload.off_boresight_deg;
    state.snapshot.predicted_intercept_valid = payload.predicted_intercept_valid;
    state.snapshot.predicted_intercept_position = {payload.predicted_intercept_x, payload.predicted_intercept_y};
    state.snapshot.time_to_intercept_s = payload.time_to_intercept_s;
    state.snapshot.track.active = payload.track_active;
    state.snapshot.track.estimated_position = {payload.track_estimated_x, payload.track_estimated_y};
    state.snapshot.track.estimated_velocity = {payload.track_estimated_vx, payload.track_estimated_vy};
    state.snapshot.track.measurement_valid = payload.track_measurement_valid;
    state.snapshot.track.measurement_position = {payload.track_measurement_x, payload.track_measurement_y};
    state.snapshot.track.measurement_residual_distance = payload.track_measurement_residual_distance;
    state.snapshot.track.covariance_trace = payload.track_covariance_trace;
    state.snapshot.track.measurement_age_ticks = static_cast<std::uint32_t>(std::max(payload.track_measurement_age_ticks, 0));
    state.snapshot.track.missed_updates = static_cast<std::uint32_t>(std::max(payload.track_missed_updates, 0));
    state.snapshot.interceptor_status = parse_interceptor_status(payload.interceptor_status);
    state.snapshot.engage_order_status = parse_engage_order_status(payload.engage_order_status);
    state.snapshot.assessment.ready = payload.assessment_ready;
    state.snapshot.assessment.code = parse_assessment_code(payload.assessment_code);
    state.snapshot.launch_angle_deg = payload.launch_angle_deg;
    if (state.snapshot.phase == icss::core::SessionPhase::Standby
        || state.snapshot.phase == icss::core::SessionPhase::Detecting
        || state.snapshot.phase == icss::core::SessionPhase::Tracking
        || state.snapshot.phase == icss::core::SessionPhase::InterceptorReady) {
        state.effective_track_active = payload.track_active;
    }
    state.aar.available = aar_available(state);
    if (!state.aar.available) {
        state.aar = {};
        state.timeline_scroll_lines = 0;
    }
    state.received_snapshot = true;
    ++state.snapshot_count_received;
    const auto target_history_point = state.snapshot.target_world_position;
    const auto interceptor_history_point = state.snapshot.interceptor_world_position;
    if (state.target_history.empty()
        || std::abs(state.target_history.back().x - target_history_point.x) > 0.01F
        || std::abs(state.target_history.back().y - target_history_point.y) > 0.01F) {
        state.target_history.push_back(target_history_point);
        while (state.target_history.size() > 24) {
            state.target_history.pop_front();
        }
    }
    if (state.interceptor_history.empty()
        || std::abs(state.interceptor_history.back().x - interceptor_history_point.x) > 0.01F
        || std::abs(state.interceptor_history.back().y - interceptor_history_point.y) > 0.01F) {
        state.interceptor_history.push_back(interceptor_history_point);
        while (state.interceptor_history.size() > 24) {
            state.interceptor_history.pop_front();
        }
    }
    if (state.snapshot.phase == icss::core::SessionPhase::Standby) {
        state.effective_track_active = false;
        state.target_history.clear();
        state.interceptor_history.clear();
    }
}

void apply_telemetry(ViewerState& state, const icss::protocol::TelemetryPayload& payload) {
    if (state.received_telemetry) {
        if (payload.sample.last_snapshot_timestamp_ms < state.snapshot.telemetry.last_snapshot_timestamp_ms) {
            return;
        }
        if (payload.sample.last_snapshot_timestamp_ms == state.snapshot.telemetry.last_snapshot_timestamp_ms
            && payload.event_tick < state.last_server_event_tick) {
            return;
        }
    }
    state.snapshot.telemetry = payload.sample;
    state.snapshot.display_connection = parse_connection_state(payload.connection_state);
    state.received_telemetry = true;
    ++state.telemetry_count_received;
    if (payload.event_type != "none"
        && (payload.event_tick != state.last_server_event_tick
            || payload.event_type != state.last_server_event_type
            || payload.event_summary != state.last_server_event_summary)) {
        state.last_server_event_tick = payload.event_tick;
        state.last_server_event_type = payload.event_type;
        state.last_server_event_summary = payload.event_summary;
        push_timeline_entry(
            state,
            "[tick " + std::to_string(payload.event_tick) + "] " + payload.event_summary + " (" + payload.event_type + ")");
    }
}

}  // namespace icss::viewer_gui
