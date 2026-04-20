#include "app.hpp"

#include <algorithm>
#include <random>

namespace icss::viewer_gui {
namespace {

int clamp_int(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

std::string setup_message(std::string_view field, int value) {
    return std::string("next start ") + std::string(field) + " = " + std::to_string(value);
}

}  // namespace

bool apply_parameter_action(ViewerState& state, std::string_view action) {
    auto& scenario = state.planned_scenario;
    auto update_preview = [&](std::string_view field, int value) {
        const bool preview_allowed = !state.received_snapshot
            || state.snapshot.phase == icss::core::SessionPhase::Standby;
        if (preview_allowed) {
            sync_preview_from_planned_scenario(state);
        }
        state.control.last_ok = true;
        state.control.last_label = "Setup";
        state.control.last_message = setup_message(field, value);
        state.aar.visible = false;
        push_timeline_entry(state, "[setup] " + std::string(field) + " -> " + std::to_string(value));
    };

    if (action == "target_pos_x_dec") {
        scenario.target_start_x = clamp_int(scenario.target_start_x - 16, 0, scenario.world_width - 1);
        update_preview("target_start_x", scenario.target_start_x);
        return true;
    }
    if (action == "target_pos_x_inc") {
        scenario.target_start_x = clamp_int(scenario.target_start_x + 16, 0, scenario.world_width - 1);
        update_preview("target_start_x", scenario.target_start_x);
        return true;
    }
    if (action == "target_pos_y_dec") {
        scenario.target_start_y = clamp_int(scenario.target_start_y - 16, 0, scenario.world_height - 1);
        update_preview("target_start_y", scenario.target_start_y);
        return true;
    }
    if (action == "target_pos_y_inc") {
        scenario.target_start_y = clamp_int(scenario.target_start_y + 16, 0, scenario.world_height - 1);
        update_preview("target_start_y", scenario.target_start_y);
        return true;
    }
    if (action == "target_vel_x_dec") {
        scenario.target_velocity_x = clamp_int(scenario.target_velocity_x - 1, -12, 12);
        update_preview("target_velocity_x", scenario.target_velocity_x);
        return true;
    }
    if (action == "target_vel_x_inc") {
        scenario.target_velocity_x = clamp_int(scenario.target_velocity_x + 1, -12, 12);
        update_preview("target_velocity_x", scenario.target_velocity_x);
        return true;
    }
    if (action == "target_vel_y_dec") {
        scenario.target_velocity_y = clamp_int(scenario.target_velocity_y - 1, -12, 12);
        update_preview("target_velocity_y", scenario.target_velocity_y);
        return true;
    }
    if (action == "target_vel_y_inc") {
        scenario.target_velocity_y = clamp_int(scenario.target_velocity_y + 1, -12, 12);
        update_preview("target_velocity_y", scenario.target_velocity_y);
        return true;
    }
    if (action == "interceptor_pos_x_dec") {
        scenario.interceptor_start_x = 0;
        update_preview("interceptor_start_x", scenario.interceptor_start_x);
        return true;
    }
    if (action == "interceptor_pos_x_inc") {
        scenario.interceptor_start_x = 0;
        update_preview("interceptor_start_x", scenario.interceptor_start_x);
        return true;
    }
    if (action == "interceptor_pos_y_dec") {
        scenario.interceptor_start_y = 0;
        update_preview("interceptor_start_y", scenario.interceptor_start_y);
        return true;
    }
    if (action == "interceptor_pos_y_inc") {
        scenario.interceptor_start_y = 0;
        update_preview("interceptor_start_y", scenario.interceptor_start_y);
        return true;
    }
    if (action == "launch_angle_dec") {
        scenario.launch_angle_deg = clamp_int(scenario.launch_angle_deg - 1, 0, 90);
        update_preview("launch_angle_deg", scenario.launch_angle_deg);
        return true;
    }
    if (action == "launch_angle_inc") {
        scenario.launch_angle_deg = clamp_int(scenario.launch_angle_deg + 1, 0, 90);
        update_preview("launch_angle_deg", scenario.launch_angle_deg);
        return true;
    }
    if (action == "interceptor_speed_dec") {
        scenario.interceptor_speed_per_tick = clamp_int(scenario.interceptor_speed_per_tick - 8, 4, 96);
        update_preview("interceptor_speed_per_tick", scenario.interceptor_speed_per_tick);
        return true;
    }
    if (action == "interceptor_speed_inc") {
        scenario.interceptor_speed_per_tick = clamp_int(scenario.interceptor_speed_per_tick + 8, 4, 96);
        update_preview("interceptor_speed_per_tick", scenario.interceptor_speed_per_tick);
        return true;
    }
    if (action == "timeout_dec") {
        scenario.engagement_timeout_ticks = clamp_int(scenario.engagement_timeout_ticks - 10, 10, 180);
        update_preview("engagement_timeout_ticks", scenario.engagement_timeout_ticks);
        return true;
    }
    if (action == "timeout_inc") {
        scenario.engagement_timeout_ticks = clamp_int(scenario.engagement_timeout_ticks + 10, 10, 180);
        update_preview("engagement_timeout_ticks", scenario.engagement_timeout_ticks);
        return true;
    }
    return false;
}

bool is_live_control_action(std::string_view action) {
    return action == "Start"
        || action == "Track"
        || action == "Ready"
        || action == "Engage"
        || action == "Reset"
        || action == "AAR";
}

std::string recommended_control_label(const ViewerState& state) {
    switch (state.snapshot.phase) {
    case icss::core::SessionPhase::Standby:
        return "Start";
    case icss::core::SessionPhase::Detecting:
        return "Track";
    case icss::core::SessionPhase::Tracking:
        return "Ready";
    case icss::core::SessionPhase::InterceptorReady:
        return state.snapshot.track.active ? "Engage" : "Track";
    case icss::core::SessionPhase::EngageOrdered:
    case icss::core::SessionPhase::Intercepting:
        return "";
    case icss::core::SessionPhase::Assessed:
    case icss::core::SessionPhase::Archived:
        if (aar_available(state) && !state.aar.loaded) {
            return "Review";
        }
        return "Reset";
    }
    return "";
}

void sync_preview_from_scenario(ViewerState& state, const icss::core::ScenarioConfig& scenario) {
    state.snapshot.world_width = scenario.world_width;
    state.snapshot.world_height = scenario.world_height;
    state.snapshot.target.position = {scenario.target_start_x, scenario.target_start_y};
    state.snapshot.interceptor.position = {scenario.interceptor_start_x, scenario.interceptor_start_y};
    state.snapshot.target_world_position = {static_cast<float>(scenario.target_start_x),
                                            static_cast<float>(scenario.target_start_y)};
    state.snapshot.interceptor_world_position = {static_cast<float>(scenario.interceptor_start_x),
                                           static_cast<float>(scenario.interceptor_start_y)};
    state.snapshot.target_velocity_x = scenario.target_velocity_x;
    state.snapshot.target_velocity_y = scenario.target_velocity_y;
    state.snapshot.target_velocity = {static_cast<float>(scenario.target_velocity_x),
                                      static_cast<float>(scenario.target_velocity_y)};
    state.snapshot.interceptor_velocity = {0.0F, 0.0F};
    state.snapshot.target_heading_deg = heading_deg_gui(state.snapshot.target_velocity);
    state.snapshot.interceptor_heading_deg = 0.0F;
    state.snapshot.interceptor_speed_per_tick = scenario.interceptor_speed_per_tick;
    state.snapshot.interceptor_acceleration_per_tick =
        std::max(1.0F, static_cast<float>(scenario.interceptor_speed_per_tick) * 0.28F);
    state.snapshot.intercept_radius = scenario.intercept_radius;
    state.snapshot.engagement_timeout_ticks = scenario.engagement_timeout_ticks;
    state.snapshot.seeker_fov_deg = static_cast<float>(scenario.seeker_fov_deg);
    state.snapshot.launch_angle_deg = static_cast<float>(scenario.launch_angle_deg);
    state.snapshot.seeker_lock = false;
    state.snapshot.off_boresight_deg = 0.0F;
    state.snapshot.predicted_intercept_valid = false;
    state.snapshot.predicted_intercept_position = {};
    state.snapshot.time_to_intercept_s = 0.0F;
    state.snapshot.track = {};
}

icss::core::ScenarioConfig randomize_start_scenario(const icss::core::ScenarioConfig& base, std::uint64_t seed) {
    std::mt19937 rng(static_cast<std::uint32_t>(seed ^ 0x9E3779B97F4A7C15ULL));
    auto jitter = [&](int value, int min_value, int max_value, int spread) {
        std::uniform_int_distribution<int> dist(-spread, spread);
        return clamp_int(value + dist(rng), min_value, max_value);
    };

    auto randomized = base;
    randomized.target_start_x = jitter(base.target_start_x, 0, base.world_width - 1, 24);
    randomized.target_start_y = jitter(base.target_start_y, 0, base.world_height - 1, 24);
    randomized.target_velocity_x = jitter(base.target_velocity_x, -12, 12, 1);
    randomized.target_velocity_y = jitter(base.target_velocity_y, -12, 12, 1);

    if (randomized.target_velocity_x == 0 && randomized.target_velocity_y == 0) {
        randomized.target_velocity_x = base.target_velocity_x == 0 ? 1 : (base.target_velocity_x > 0 ? 1 : -1);
    }
    if (randomized.target_start_x == base.target_start_x
        && randomized.target_start_y == base.target_start_y
        && randomized.target_velocity_x == base.target_velocity_x
        && randomized.target_velocity_y == base.target_velocity_y) {
        randomized.target_start_x = clamp_int(base.target_start_x + 16, 0, base.world_width - 1);
    }
    return randomized;
}

void sync_preview_from_planned_scenario(ViewerState& state) {
    sync_preview_from_scenario(state, state.planned_scenario);
}

}  // namespace icss::viewer_gui
