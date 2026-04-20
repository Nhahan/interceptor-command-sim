#include <cassert>
#include <string>

#include "clients/tactical-viewer-gui/src/app.hpp"
#include "icss/protocol/payloads.hpp"

namespace {

icss::protocol::SnapshotPayload make_snapshot(std::uint64_t sequence,
                                              std::uint64_t tick,
                                              float target_world_x,
                                              int target_x,
                                              const char* phase = "detecting") {
    icss::protocol::SnapshotPayload payload;
    payload.header.snapshot_sequence = sequence;
    payload.header.tick = tick;
    payload.header.timestamp_ms = 1000U + tick;
    payload.phase = phase;
    payload.world_width = 576;
    payload.world_height = 384;
    payload.target_id = "target-alpha";
    payload.target_active = true;
    payload.target_x = target_x;
    payload.target_y = 300;
    payload.target_world_x = target_world_x;
    payload.target_world_y = 300.0F;
    payload.target_velocity_x = 5;
    payload.target_velocity_y = -3;
    payload.target_velocity_world_x = 5.0F;
    payload.target_velocity_world_y = -3.0F;
    payload.target_heading_deg = -31.0F;
    payload.interceptor_id = "interceptor-alpha";
    payload.interceptor_active = false;
    payload.interceptor_x = 160;
    payload.interceptor_y = 60;
    payload.interceptor_world_x = 160.0F;
    payload.interceptor_world_y = 60.0F;
    payload.interceptor_velocity_world_x = 0.0F;
    payload.interceptor_velocity_world_y = 0.0F;
    payload.interceptor_heading_deg = 0.0F;
    payload.interceptor_speed_per_tick = 32;
    payload.interceptor_acceleration_per_tick = 8.0F;
    payload.intercept_radius = 24;
    payload.engagement_timeout_ticks = 60;
    payload.seeker_fov_deg = 45.0F;
    payload.seeker_lock = false;
    payload.off_boresight_deg = 0.0F;
    payload.predicted_intercept_valid = false;
    payload.time_to_intercept_s = 0.0F;
    payload.track_active = false;
    payload.interceptor_status = "idle";
    payload.engage_order_status = "none";
    payload.assessment_ready = false;
    payload.assessment_code = "pending";
    return payload;
}

icss::protocol::TelemetryPayload make_telemetry(std::uint64_t tick,
                                                std::uint32_t tick_interval_ms,
                                                std::uint64_t event_tick,
                                                const char* event_type,
                                                const char* summary) {
    icss::protocol::TelemetryPayload payload;
    payload.sample.tick = tick;
    payload.sample.tick_interval_ms = tick_interval_ms;
    payload.sample.packet_loss_pct = 0.0F;
    payload.sample.last_snapshot_timestamp_ms = 5000U + tick;
    payload.connection_state = "connected";
    payload.event_tick = event_tick;
    payload.event_type = event_type;
    payload.event_summary = summary;
    return payload;
}

}  // namespace

int main() {
    using namespace icss::viewer_gui;

    ViewerState state;
    state.aar.available = true;
    state.aar.loaded = true;
    state.aar.visible = true;
    state.aar.assessment_code = "intercept_success";
    state.timeline_scroll_lines = 9;

    apply_snapshot(state, make_snapshot(5U, 10U, 110.0F, 110));
    assert(state.received_snapshot);
    assert(state.snapshot.header.snapshot_sequence == 5U);
    assert(state.snapshot.telemetry.tick == 0U);
    assert(state.snapshot.target_world_position.x == 110.0F);
    assert(state.target_history.size() == 1);
    assert(!state.aar.available);
    assert(!state.aar.loaded);
    assert(!state.aar.visible);
    assert(state.timeline_scroll_lines == 0U);

    apply_snapshot(state, make_snapshot(4U, 9U, 90.0F, 90));
    assert(state.snapshot.header.snapshot_sequence == 5U);
    assert(state.snapshot.target_world_position.x == 110.0F);
    assert(state.target_history.size() == 1);
    assert(state.snapshot_count_received == 1U);

    apply_snapshot(state, make_snapshot(6U, 11U, 130.0F, 130));
    assert(state.snapshot.header.snapshot_sequence == 6U);
    assert(state.snapshot.target_world_position.x == 130.0F);
    assert(state.target_history.size() == 2);
    assert(state.snapshot_count_received == 2U);

    apply_telemetry(state, make_telemetry(11U, 33U, 11U, "track_updated", "track ok"));
    assert(state.received_telemetry);
    assert(state.snapshot.telemetry.tick == 11U);
    assert(state.snapshot.telemetry.tick_interval_ms == 33U);
    assert(state.last_server_event_type == "track_updated");
    assert(state.recent_server_events.size() == 1);

    apply_telemetry(state, make_telemetry(9U, 99U, 9U, "client_left", "stale telemetry"));
    assert(state.snapshot.telemetry.tick == 11U);
    assert(state.snapshot.telemetry.tick_interval_ms == 33U);
    assert(state.last_server_event_type == "track_updated");
    assert(state.recent_server_events.size() == 1);

    auto reset_style_telemetry = make_telemetry(0U, 19U, 0U, "session_started", "reset snapshot");
    reset_style_telemetry.sample.last_snapshot_timestamp_ms = 7000U;
    apply_telemetry(state, reset_style_telemetry);
    assert(state.snapshot.telemetry.tick == 0U);
    assert(state.snapshot.telemetry.tick_interval_ms == 19U);
    assert(state.last_server_event_type == "session_started");
    assert(state.recent_server_events.size() == 2);

    auto newer_after_reset = make_telemetry(12U, 21U, 12U, "interceptor_updated", "interceptor ready");
    newer_after_reset.sample.last_snapshot_timestamp_ms = 7100U;
    apply_telemetry(state, newer_after_reset);
    assert(state.snapshot.telemetry.tick == 12U);
    assert(state.snapshot.telemetry.tick_interval_ms == 21U);
    assert(state.last_server_event_type == "interceptor_updated");
    assert(state.recent_server_events.size() == 3);

    auto same_snapshot_older_event = make_telemetry(12U, 55U, 10U, "client_left", "older event same snapshot");
    same_snapshot_older_event.sample.last_snapshot_timestamp_ms = 7100U;
    apply_telemetry(state, same_snapshot_older_event);
    assert(state.snapshot.telemetry.tick == 12U);
    assert(state.snapshot.telemetry.tick_interval_ms == 21U);
    assert(state.last_server_event_type == "interceptor_updated");
    assert(state.recent_server_events.size() == 3);

    auto same_snapshot_newer_event = make_telemetry(12U, 19U, 13U, "engage_order_accepted", "newer event same snapshot");
    same_snapshot_newer_event.sample.last_snapshot_timestamp_ms = 7100U;
    apply_telemetry(state, same_snapshot_newer_event);
    assert(state.snapshot.telemetry.tick == 12U);
    assert(state.snapshot.telemetry.tick_interval_ms == 19U);
    assert(state.last_server_event_type == "engage_order_accepted");
    assert(state.recent_server_events.size() == 4);

    return 0;
}
