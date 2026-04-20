#include <algorithm>
#include <cassert>
#include <string>

#include "icss/core/simulation.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    using namespace icss::core;
    using namespace icss::protocol;

    SimulationSession session;
    session.connect_client(ClientRole::FireControlConsole, 101U);
    session.connect_client(ClientRole::TacticalDisplay, 201U);
    session.start_scenario();
    session.timeout_client(ClientRole::TacticalDisplay, "heartbeat timeout threshold exceeded");

    const auto& events = session.events();
    const auto timeout_seen = std::any_of(events.begin(), events.end(), [](const EventRecord& event) {
        return event.summary == "Client timeout exercised" && event.header.event_type == EventType::ResilienceTriggered;
    });

    assert(timeout_seen);
    assert(session.latest_snapshot().display_connection == ConnectionState::TimedOut);
    assert(session.build_summary().resilience_case == "timeout_visibility");
    assert(session.build_summary().assessment_code == AssessmentCode::Pending);
    const auto frame = icss::view::render_tactical_frame(
        session.latest_snapshot(),
        events,
        icss::view::make_replay_cursor(events.size(), events.empty() ? 0 : events.size() - 1));
    assert(frame.find("connection=timed_out") != std::string::npos);
    assert(frame.find("freshness=stale") != std::string::npos);

    SimulationSession reset_session;
    reset_session.connect_client(ClientRole::FireControlConsole, 101U);
    reset_session.reset_session("idle reset");
    const auto idle_frame = icss::view::render_tactical_frame(
        reset_session.latest_snapshot(),
        reset_session.events(),
        icss::view::make_replay_cursor(reset_session.events().size(),
                                       reset_session.events().empty() ? 0 : reset_session.events().size() - 1));
    const auto entities_pos = idle_frame.find("Entities:\n");
    assert(entities_pos != std::string::npos);
    const auto grid_start = idle_frame.find('\n');
    assert(grid_start != std::string::npos);
    const auto grid_block = idle_frame.substr(grid_start + 1, entities_pos - (grid_start + 1));
    assert(grid_block.find('T') == std::string::npos);
    assert(grid_block.find('A') == std::string::npos);
    return 0;
}
