#include <filesystem>
#include <iostream>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    namespace view = icss::view;

    auto transport = make_transport(BackendKind::InProcess, default_runtime_config(fs::path{ICSS_REPO_ROOT}));
    transport->connect_client(ClientRole::FireControlConsole, 101U);
    transport->connect_client(ClientRole::TacticalDisplay, 201U);
    transport->start_scenario();
    transport->dispatch(icss::protocol::TrackAcquirePayload{{1001U, 101U, 1U}, "target-alpha"});
    transport->advance_tick();
    transport->dispatch(icss::protocol::InterceptorReadyPayload{{1001U, 101U, 2U}, "interceptor-alpha"});
    transport->dispatch(icss::protocol::EngageOrderPayload{{1001U, 101U, 3U}, "interceptor-alpha", "target-alpha"});
    for (int i = 0; i < transport->latest_snapshot().engagement_timeout_ticks && !transport->latest_snapshot().assessment.ready; ++i) {
        transport->advance_tick();
    }
    transport->archive_session();

    const auto events = transport->events();
    const auto snapshot = transport->latest_snapshot();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    std::vector<EventRecord> recent(events.begin() + static_cast<std::ptrdiff_t>(start), events.end());
    auto cursor = view::make_replay_cursor(events.size(), events.size() > 1 ? events.size() - 2 : 0);
    cursor = view::step_forward(cursor);

    const icss::protocol::SnapshotPayload snapshot_payload {
        snapshot.envelope,
        snapshot.header,
        to_string(snapshot.phase),
        snapshot.world_width,
        snapshot.world_height,
        snapshot.target.id,
        snapshot.target.active,
        snapshot.target.position.x,
        snapshot.target.position.y,
        snapshot.target_world_position.x,
        snapshot.target_world_position.y,
        snapshot.target_velocity_x,
        snapshot.target_velocity_y,
        snapshot.target_velocity.x,
        snapshot.target_velocity.y,
        snapshot.target_heading_deg,
        snapshot.interceptor.id,
        snapshot.interceptor.active,
        snapshot.interceptor.position.x,
        snapshot.interceptor.position.y,
        snapshot.interceptor_world_position.x,
        snapshot.interceptor_world_position.y,
        snapshot.interceptor_velocity.x,
        snapshot.interceptor_velocity.y,
        snapshot.interceptor_heading_deg,
        snapshot.interceptor_speed_per_tick,
        snapshot.interceptor_acceleration_per_tick,
        snapshot.intercept_radius,
        snapshot.engagement_timeout_ticks,
        snapshot.seeker_fov_deg,
        snapshot.seeker_lock,
        snapshot.off_boresight_deg,
        snapshot.predicted_intercept_valid,
        snapshot.predicted_intercept_position.x,
        snapshot.predicted_intercept_position.y,
        snapshot.time_to_intercept_s,
        snapshot.track.active,
        snapshot.track.estimated_position.x,
        snapshot.track.estimated_position.y,
        snapshot.track.estimated_velocity.x,
        snapshot.track.estimated_velocity.y,
        snapshot.track.measurement_valid,
        snapshot.track.measurement_position.x,
        snapshot.track.measurement_position.y,
        snapshot.track.measurement_residual_distance,
        snapshot.track.covariance_trace,
        static_cast<int>(snapshot.track.measurement_age_ticks),
        static_cast<int>(snapshot.track.missed_updates),
        to_string(snapshot.interceptor_status),
        to_string(snapshot.engage_order_status),
        snapshot.assessment.ready,
        to_string(snapshot.assessment.code),
    };
    const icss::protocol::TelemetryPayload telemetry_payload {
        snapshot.envelope,
        snapshot.telemetry,
        to_string(snapshot.display_connection),
        recent.empty() ? 0U : recent.back().header.tick,
        recent.empty() ? "none" : std::string(icss::protocol::to_string(recent.back().header.event_type)),
        recent.empty() ? "no server event" : recent.back().summary,
    };

    std::cout << "backend=" << transport->backend_name() << '\n';
    std::cout << view::render_tactical_frame(snapshot, recent, cursor);
    std::cout << "wire_preview.snapshot=" << icss::protocol::serialize(snapshot_payload) << '\n';
    std::cout << "wire_preview.telemetry=" << icss::protocol::serialize(telemetry_payload) << '\n';
    return 0;
}
