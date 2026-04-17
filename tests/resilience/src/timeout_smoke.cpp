#include <algorithm>
#include <cassert>
#include <string>

#include "icss/core/simulation.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    using namespace icss::core;
    using namespace icss::protocol;

    SimulationSession session;
    session.connect_client(ClientRole::CommandConsole, 101U);
    session.connect_client(ClientRole::TacticalViewer, 201U);
    session.start_scenario();
    session.timeout_client(ClientRole::TacticalViewer, "heartbeat timeout threshold exceeded");

    const auto& events = session.events();
    const auto timeout_seen = std::any_of(events.begin(), events.end(), [](const EventRecord& event) {
        return event.summary == "Client timeout exercised" && event.header.event_type == EventType::ResilienceTriggered;
    });

    assert(timeout_seen);
    assert(session.latest_snapshot().viewer_connection == ConnectionState::TimedOut);
    assert(session.build_summary().resilience_case == "timeout_visibility");
    assert(session.build_summary().judgment_code == JudgmentCode::Pending);
    const auto frame = icss::view::render_tactical_frame(
        session.latest_snapshot(),
        events,
        icss::view::make_replay_cursor(events.size(), events.empty() ? 0 : events.size() - 1));
    assert(frame.find("connection=timed_out") != std::string::npos);
    assert(frame.find("freshness=stale") != std::string::npos);
    return 0;
}
