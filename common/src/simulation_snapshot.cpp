#include "icss/core/simulation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "simulation_internal.hpp"

namespace icss::core {
namespace {

std::uint32_t tick_interval_ms_for_rate(int tick_rate_hz) {
    return static_cast<std::uint32_t>(std::max(1, 1000 / std::max(1, tick_rate_hz)));
}

std::uint64_t wall_time_ms_now() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

}  // namespace

void SimulationSession::record_snapshot(float packet_loss_pct) {
    ++sequence_;
    const auto timestamp = next_timestamp_ms();
    const auto capture_wall_time_ms = wall_time_ms_now();
    const auto display_connection = tactical_display_.connection;
    const auto target_heading = detail::heading_deg(target_velocity_world_);
    const auto asset_heading = detail::heading_deg(interceptor_velocity_world_);
    const bool engagement_context = asset_.active
        && (engage_order_status_ == EngageOrderStatus::Accepted
            || engage_order_status_ == EngageOrderStatus::Executing
            || engage_order_status_ == EngageOrderStatus::Completed
            || assessment_.ready);
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
        {tick_, timestamp, sequence_, capture_wall_time_ms},
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
        interceptor_velocity_world_,
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
        interceptor_status_,
        engage_order_status_,
        assessment_,
        display_connection,
        {tick_, tick_interval_ms_for_rate(tick_rate_hz_), packet_loss_pct, timestamp, capture_wall_time_ms},
        static_cast<float>(scenario_.launch_angle_deg),
    });
    if (display_connection == ConnectionState::Reconnected) {
        tactical_display_.connection = ConnectionState::Connected;
    }
}

}  // namespace icss::core
