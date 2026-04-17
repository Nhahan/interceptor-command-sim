#include <cassert>

#include "icss/core/config.hpp"
#include "icss/core/simulation.hpp"

int main() {
    using namespace icss::core;

    ScenarioConfig scenario;
    scenario.world_width = 24;
    scenario.world_height = 16;
    scenario.target_start_x = 3;
    scenario.target_start_y = 13;
    scenario.target_velocity_x = 1;
    scenario.target_velocity_y = -1;
    scenario.interceptor_start_x = 10;
    scenario.interceptor_start_y = 2;
    scenario.interceptor_speed_per_tick = 4;
    scenario.intercept_radius = 1;
    scenario.engagement_timeout_ticks = 8;

    SimulationSession early(1001U, 20, 200, scenario);
    early.connect_client(ClientRole::CommandConsole, 101U);
    early.connect_client(ClientRole::TacticalViewer, 201U);
    assert(early.start_scenario().accepted);
    assert(early.request_track().accepted);
    early.advance_tick();
    assert(early.activate_asset().accepted);
    assert(early.issue_command().accepted);
    for (int i = 0; i < scenario.engagement_timeout_ticks && !early.latest_snapshot().judgment.ready; ++i) {
        early.advance_tick();
    }
    assert(early.latest_snapshot().judgment.ready);
    assert(early.latest_snapshot().judgment.code == JudgmentCode::InterceptSuccess);

    ScenarioConfig late_scenario = scenario;
    late_scenario.interceptor_speed_per_tick = 1;
    late_scenario.engagement_timeout_ticks = 2;

    SimulationSession late(1002U, 20, 200, late_scenario);
    late.connect_client(ClientRole::CommandConsole, 101U);
    late.connect_client(ClientRole::TacticalViewer, 201U);
    assert(late.start_scenario().accepted);
    assert(late.request_track().accepted);
    for (int i = 0; i < 6; ++i) {
        late.advance_tick();
    }
    assert(late.activate_asset().accepted);
    assert(late.issue_command().accepted);
    for (int i = 0; i < late_scenario.engagement_timeout_ticks && !late.latest_snapshot().judgment.ready; ++i) {
        late.advance_tick();
    }
    assert(late.latest_snapshot().judgment.ready);
    assert(late.latest_snapshot().judgment.code == JudgmentCode::TimeoutObserved);
    return 0;
}
