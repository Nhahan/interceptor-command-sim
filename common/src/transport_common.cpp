#include "transport_internal.hpp"

#include <string>

namespace icss::net::detail {

icss::protocol::FrameFormat parse_frame_format(std::string_view value) {
    if (value == "json") {
        return icss::protocol::FrameFormat::JsonLine;
    }
    if (value == "binary") {
        return icss::protocol::FrameFormat::Binary;
    }
    throw std::runtime_error("unsupported tcp_frame_format: " + std::string(value));
}

std::vector<icss::core::EventRecord> recent_events(const icss::core::SimulationSession& session) {
    std::vector<icss::core::EventRecord> recent;
    const auto& events = session.events();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    for (std::size_t index = start; index < events.size(); ++index) {
        recent.push_back(events[index]);
    }
    return recent;
}

const icss::core::EventRecord* latest_event_at_or_before_tick(const std::vector<icss::core::EventRecord>& events,
                                                              std::uint64_t tick) {
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        if (it->header.tick <= tick) {
            return &*it;
        }
    }
    return nullptr;
}

icss::protocol::SnapshotPayload to_snapshot_payload(const icss::core::Snapshot& snapshot) {
    return {
        snapshot.envelope,
        snapshot.header,
        icss::core::to_string(snapshot.phase),
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
        snapshot.asset.id,
        snapshot.asset.active,
        snapshot.asset.position.x,
        snapshot.asset.position.y,
        snapshot.asset_world_position.x,
        snapshot.asset_world_position.y,
        snapshot.asset_velocity.x,
        snapshot.asset_velocity.y,
        snapshot.asset_heading_deg,
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
        icss::core::to_string(snapshot.asset_status),
        icss::core::to_string(snapshot.command_status),
        snapshot.judgment.ready,
        icss::core::to_string(snapshot.judgment.code),
        snapshot.launch_angle_deg,
    };
}

icss::protocol::TelemetryPayload to_telemetry_payload(const icss::core::Snapshot& snapshot,
                                                      const std::vector<icss::core::EventRecord>& events) {
    std::uint64_t event_tick = 0;
    std::string event_type = "none";
    std::string event_summary = "no server event";
    if (const auto* event = latest_event_at_or_before_tick(events, snapshot.header.tick)) {
        event_tick = event->header.tick;
        event_type = std::string(icss::protocol::to_string(event->header.event_type));
        event_summary = event->summary;
    }
    return {
        snapshot.envelope,
        snapshot.telemetry,
        icss::core::to_string(snapshot.viewer_connection),
        event_tick,
        std::move(event_type),
        std::move(event_summary),
    };
}

}  // namespace icss::net::detail
