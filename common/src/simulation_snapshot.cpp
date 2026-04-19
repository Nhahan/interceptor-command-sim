#include "icss/core/simulation.hpp"

#include <algorithm>
#include <cmath>

#include "simulation_internal.hpp"

namespace icss::core {

void SimulationSession::record_snapshot(float packet_loss_pct) {
    ++sequence_;
    const auto timestamp = next_timestamp_ms();
    const auto viewer_connection = tactical_viewer_.connection;
    const auto target_heading = detail::heading_deg(target_velocity_world_);
    const auto asset_heading = detail::heading_deg(asset_velocity_world_);
    const bool engagement_context = asset_.active
        && (command_status_ == CommandLifecycle::Accepted
            || command_status_ == CommandLifecycle::Executing
            || command_status_ == CommandLifecycle::Completed
            || judgment_.ready);
    auto predicted = Vec2f {};
    auto tti = 0.0F;
    auto predicted_valid = false;
    auto seeker_lock = false;
    auto off_boresight = 0.0F;
    if (engagement_context && track_.active) {
        const auto relative = detail::subtract(target_world_, asset_world_);
        const auto distance = detail::length(relative);
        const auto max_speed = std::max(0.1F, static_cast<float>(scenario_.interceptor_speed_per_tick));
        tti = distance / max_speed;
        predicted = detail::add(target_world_, detail::scale(target_velocity_world_, tti));
        predicted_valid = distance > 0.0F;
        seeker_lock = seeker_lock_;
        off_boresight = off_boresight_deg_;
    }
    snapshots_.push_back({
        {session_id_, kServerSenderId, sequence_},
        {tick_, timestamp, sequence_},
        phase_,
        scenario_.world_width,
        scenario_.world_height,
        target_,
        asset_,
        target_world_,
        asset_world_,
        static_cast<int>(std::lround(target_velocity_world_.x)),
        static_cast<int>(std::lround(target_velocity_world_.y)),
        target_velocity_world_,
        asset_velocity_world_,
        target_heading,
        asset_heading,
        scenario_.interceptor_speed_per_tick,
        std::max(1.0F, static_cast<float>(scenario_.interceptor_speed_per_tick) * 0.28F),
        scenario_.intercept_radius,
        scenario_.engagement_timeout_ticks,
        static_cast<float>(scenario_.seeker_fov_deg),
        seeker_lock,
        off_boresight,
        predicted_valid,
        predicted,
        tti,
        track_,
        asset_status_,
        command_status_,
        judgment_,
        viewer_connection,
        {tick_, static_cast<std::uint32_t>((telemetry_interval_ms_ / 10) + tick_rate_hz_ + static_cast<int>(tick_)), packet_loss_pct, timestamp},
        static_cast<float>(scenario_.launch_angle_deg),
    });
    if (viewer_connection == ConnectionState::Reconnected) {
        tactical_viewer_.connection = ConnectionState::Connected;
    }
}

}  // namespace icss::core
