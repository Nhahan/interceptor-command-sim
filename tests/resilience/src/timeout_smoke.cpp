#include <algorithm>
#include <cassert>
#include <string>

#include "icss/core/simulation.hpp"

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
    return 0;
}
