#include "app.hpp"

#include <cmath>
#include <deque>

namespace icss::viewer_gui {
namespace {

bool active_live_phase(icss::core::SessionPhase phase) {
    return phase == icss::core::SessionPhase::Detecting
        || phase == icss::core::SessionPhase::Tracking
        || phase == icss::core::SessionPhase::InterceptorReady
        || phase == icss::core::SessionPhase::EngageOrdered
        || phase == icss::core::SessionPhase::Intercepting;
}

bool recent_motion_in_history(const std::deque<icss::core::Vec2f>& history) {
    if (history.size() < 2) {
        return false;
    }
    const auto& latest = history.back();
    const auto& previous = history[history.size() - 2];
    return std::abs(latest.x - previous.x) > 0.25F
        || std::abs(latest.y - previous.y) > 0.25F;
}

float speed_sq(icss::core::Vec2f velocity) {
    return (velocity.x * velocity.x) + (velocity.y * velocity.y);
}

}  // namespace

bool target_motion_visual_visible(const ViewerState& state) {
    return state.snapshot.target.active
        && state.snapshot.track.active
        && active_live_phase(state.snapshot.phase)
        && recent_motion_in_history(state.target_history)
        && speed_sq(state.snapshot.target_velocity) > 0.01F;
}

bool interceptor_motion_visual_visible(const ViewerState& state) {
    return state.snapshot.interceptor.active
        && state.snapshot.phase == icss::core::SessionPhase::Intercepting
        && state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing
        && recent_motion_in_history(state.interceptor_history)
        && speed_sq(state.snapshot.interceptor_velocity) > 0.01F;
}

bool engagement_visual_visible(const ViewerState& state) {
    return state.snapshot.target.active
        && state.snapshot.interceptor.active
        && state.snapshot.phase == icss::core::SessionPhase::Intercepting
        && state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing
        && state.snapshot.predicted_intercept_valid
        && recent_motion_in_history(state.target_history)
        && recent_motion_in_history(state.interceptor_history);
}

bool predicted_marker_visual_visible(const ViewerState& state) {
    return state.snapshot.phase == icss::core::SessionPhase::Intercepting
        && state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing
        && state.snapshot.predicted_intercept_valid;
}

bool command_visual_active(const ViewerState& state) {
    return state.snapshot.phase == icss::core::SessionPhase::Intercepting
        && state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing;
}

}  // namespace icss::viewer_gui
