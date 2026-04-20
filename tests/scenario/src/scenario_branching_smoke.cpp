#include <cassert>

#include "icss/core/config.hpp"
#include "icss/core/simulation.hpp"

int main() {
    using namespace icss::core;

    ScenarioConfig scenario;
    scenario.world_width = 576;
    scenario.world_height = 384;
    scenario.target_start_x = 80;
    scenario.target_start_y = 300;
    scenario.target_velocity_x = 5;
    scenario.target_velocity_y = -3;
    scenario.interceptor_start_x = 0;
    scenario.interceptor_start_y = 0;
    scenario.interceptor_speed_per_tick = 32;
    scenario.intercept_radius = 24;
    scenario.engagement_timeout_ticks = 60;

    SimulationSession early(1001U, 20, 200, scenario);
    early.connect_client(ClientRole::FireControlConsole, 101U);
    early.connect_client(ClientRole::TacticalDisplay, 201U);
    assert(early.start_scenario().accepted);
    assert(early.request_track().accepted);
    early.advance_tick();
    assert(early.activate_asset().accepted);
    assert(early.issue_command().accepted);
    early.advance_tick();
    const auto early_intercepting = early.latest_snapshot();
    assert(early_intercepting.predicted_intercept_valid);
    assert(early_intercepting.time_to_intercept_s > 0.0F);
    for (int i = 0; i < scenario.engagement_timeout_ticks && !early.latest_snapshot().assessment.ready; ++i) {
        early.advance_tick();
    }
    assert(early.latest_snapshot().assessment.ready);
    assert(early.latest_snapshot().assessment.code == AssessmentCode::InterceptSuccess);
    assert(!early.latest_snapshot().target.active);
    assert(early.latest_snapshot().target_velocity.x == 0.0F);
    assert(early.latest_snapshot().target_velocity.y == 0.0F);
    assert(early.latest_snapshot().interceptor_world_position.x != static_cast<float>(early.latest_snapshot().interceptor.position.x)
           || early.latest_snapshot().interceptor_world_position.y != static_cast<float>(early.latest_snapshot().interceptor.position.y));

    ScenarioConfig late_scenario = scenario;
    late_scenario.target_velocity_x = 10;
    late_scenario.target_velocity_y = -6;
    late_scenario.interceptor_speed_per_tick = 6;
    late_scenario.intercept_radius = 10;
    late_scenario.engagement_timeout_ticks = 6;

    SimulationSession late(1002U, 20, 200, late_scenario);
    late.connect_client(ClientRole::FireControlConsole, 101U);
    late.connect_client(ClientRole::TacticalDisplay, 201U);
    assert(late.start_scenario().accepted);
    assert(late.request_track().accepted);
    for (int i = 0; i < 6; ++i) {
        late.advance_tick();
    }
    assert(late.activate_asset().accepted);
    assert(late.issue_command().accepted);
    late.advance_tick();
    const auto late_intercepting = late.latest_snapshot();
    assert(late_intercepting.predicted_intercept_valid);
    for (int i = 0; i < late_scenario.engagement_timeout_ticks && !late.latest_snapshot().assessment.ready; ++i) {
        late.advance_tick();
    }
    assert(late.latest_snapshot().assessment.ready);
    assert(late.latest_snapshot().assessment.code == AssessmentCode::TimeoutObserved);

    ScenarioConfig bounce_scenario = scenario;
    bounce_scenario.world_width = 24;
    bounce_scenario.world_height = 24;
    bounce_scenario.target_start_x = 22;
    bounce_scenario.target_start_y = 12;
    bounce_scenario.target_velocity_x = 5;
    bounce_scenario.target_velocity_y = 0;

    SimulationSession bounce(1003U, 20, 200, bounce_scenario);
    bounce.connect_client(ClientRole::FireControlConsole, 101U);
    bounce.connect_client(ClientRole::TacticalDisplay, 201U);
    assert(bounce.start_scenario().accepted);
    bounce.advance_tick();
    const auto bounce_snapshot = bounce.latest_snapshot();
    assert(bounce_snapshot.target_velocity_x < 0);
    assert(bounce_snapshot.target_velocity.x < 0.0F);
    assert(bounce_snapshot.target_heading_deg > 170.0F || bounce_snapshot.target_heading_deg < -170.0F);
    return 0;
}
