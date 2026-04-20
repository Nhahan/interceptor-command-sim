#include "icss/core/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "simulation_internal.hpp"

namespace icss::core {

void SimulationSession::advance_tick() {
    if (phase_ == SessionPhase::Assessed) {
        ++tick_;
        archive_session();
        return;
    }

    ++tick_;

    if (target_.active) {
        detail::bounce_world_axis(target_world_.x, target_velocity_world_.x, static_cast<float>(scenario_.world_width));
        detail::bounce_world_axis(target_world_.y, target_velocity_world_.y, static_cast<float>(scenario_.world_height));
        target_.position.x = static_cast<int>(std::lround(target_world_.x));
        target_.position.y = static_cast<int>(std::lround(target_world_.y));
    }

    if (track_.active) {
        update_track_state();
    }

    if (engage_order_status_ == EngageOrderStatus::Accepted && phase_ == SessionPhase::EngageOrdered) {
        phase_ = SessionPhase::Intercepting;
        interceptor_status_ = InterceptorStatus::Intercepting;
        engage_order_status_ = EngageOrderStatus::Executing;
    }

    if (phase_ == SessionPhase::Intercepting && asset_.active) {
        const auto interceptor_max_speed = static_cast<float>(scenario_.interceptor_speed_per_tick);
        if (track_.active) {
            const auto to_target = detail::subtract(target_world_, asset_world_);
            const auto distance = detail::length(to_target);
            const auto interceptor_accel = std::max(1.0F, interceptor_max_speed * 0.28F);
            const auto tti_guess = distance / std::max(0.1F, interceptor_max_speed);
            const auto predicted_intercept = detail::add(target_world_, detail::scale(target_velocity_world_, tti_guess));
            const auto desired_velocity = detail::scale(detail::normalize(detail::subtract(predicted_intercept, asset_world_)), interceptor_max_speed);
            const auto velocity_error = detail::subtract(desired_velocity, interceptor_velocity_world_);
            const auto accel_step = detail::scale(detail::normalize(velocity_error), std::min(interceptor_accel, detail::length(velocity_error)));
            interceptor_velocity_world_ = detail::add(interceptor_velocity_world_, accel_step);
        }
        const auto speed = detail::length(interceptor_velocity_world_);
        if (speed > interceptor_max_speed && speed > 0.0F) {
            interceptor_velocity_world_ = detail::scale(detail::normalize(interceptor_velocity_world_), interceptor_max_speed);
        }
        asset_world_ = detail::add(asset_world_, interceptor_velocity_world_);
        asset_world_.x = detail::clampf(asset_world_.x, 0.0F, static_cast<float>(scenario_.world_width - 1));
        asset_world_.y = detail::clampf(asset_world_.y, 0.0F, static_cast<float>(scenario_.world_height - 1));
        asset_.position.x = static_cast<int>(std::lround(asset_world_.x));
        asset_.position.y = static_cast<int>(std::lround(asset_world_.y));
    }

    {
        const bool engagement_context = asset_.active
            && (engage_order_status_ == EngageOrderStatus::Accepted
                || engage_order_status_ == EngageOrderStatus::Executing
                || engage_order_status_ == EngageOrderStatus::Completed);
        if (engagement_context && track_.active) {
            const auto los = detail::subtract(target_world_, asset_world_);
            off_boresight_deg_ = std::abs(detail::normalize_angle_deg(detail::heading_deg(los) - detail::heading_deg(interceptor_velocity_world_)));
            seeker_lock_ = detail::length(los) > 0.0F && off_boresight_deg_ <= static_cast<float>(scenario_.seeker_fov_deg);
        } else {
            off_boresight_deg_ = 0.0F;
            seeker_lock_ = false;
        }
    }

    if (engage_order_status_ == EngageOrderStatus::Executing && phase_ == SessionPhase::Intercepting) {
        const auto distance = detail::length(detail::subtract(target_world_, asset_world_));
        const auto engagement_ticks = engagement_started_tick_.has_value() ? tick_ - *engagement_started_tick_ : 0;
        if (distance <= static_cast<float>(scenario_.intercept_radius)) {
            assessment_.ready = true;
            assessment_.code = AssessmentCode::InterceptSuccess;
            assessment_.summary = "authoritative intercept assessment complete";
            phase_ = SessionPhase::Assessed;
            target_.active = false;
            track_.active = false;
            target_velocity_world_ = {0.0F, 0.0F};
            interceptor_velocity_world_ = {0.0F, 0.0F};
            seeker_lock_ = false;
            off_boresight_deg_ = 0.0F;
            interceptor_status_ = InterceptorStatus::Complete;
            engage_order_status_ = EngageOrderStatus::Completed;
            engagement_started_tick_.reset();
            push_event(protocol::EventType::AssessmentProduced,
                       "simulation_server",
                       {asset_.id, target_.id},
                       "Assessment produced",
                       "Server-authoritative assessment marked the intercept attempt as successful and the target as intercepted.");
        } else if (engagement_ticks >= static_cast<std::uint64_t>(scenario_.engagement_timeout_ticks)) {
            assessment_.ready = true;
            assessment_.code = AssessmentCode::TimeoutObserved;
            assessment_.summary = "authoritative intercept timeout observed";
            phase_ = SessionPhase::Assessed;
            interceptor_status_ = InterceptorStatus::Complete;
            engage_order_status_ = EngageOrderStatus::Completed;
            engagement_started_tick_.reset();
            push_event(protocol::EventType::AssessmentProduced,
                       "simulation_server",
                       {asset_.id, target_.id},
                       "Assessment produced",
                       "Server-authoritative assessment marked the intercept attempt as timed out before intercept.");
        }
    }

    float packet_loss = 0.0F;
    if (tick_ == 2 && !packet_gap_exercised_) {
        packet_gap_exercised_ = true;
        packet_loss = 25.0F;
        push_event(protocol::EventType::ResilienceTriggered,
                   "simulation_server",
                   {target_.id},
                   "Snapshot gap exercised",
                   "Viewer should converge on the next valid snapshot rather than require retransmission.");
    }

    record_snapshot(packet_loss);
}


}  // namespace icss::core
