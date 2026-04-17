#include <algorithm>
#include <cassert>
#include <string>

#include "icss/core/simulation.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    using namespace icss::core;

    auto session = run_baseline_demo();
    const auto& events = session.events();

    const auto reconnected = std::any_of(events.begin(), events.end(), [](const EventRecord& event) {
        return event.summary == "Client reconnected";
    });
    const auto resilience = std::any_of(events.begin(), events.end(), [](const EventRecord& event) {
        return event.summary == "Reconnect/resync path exercised" || event.summary == "Snapshot gap exercised";
    });

    assert(reconnected);
    assert(resilience);

    const auto frame = icss::view::render_tactical_frame(
        session.latest_snapshot(),
        events,
        icss::view::make_replay_cursor(events.size(), events.empty() ? 0 : events.size() - 1));
    assert(frame.find("packet_loss_pct=") != std::string::npos);
    assert(frame.find("AAR:") != std::string::npos);
    assert(frame.find("cursor_index=") != std::string::npos);
    assert(frame.find("Entities:") != std::string::npos);
    assert(frame.find("freshness=") != std::string::npos);

    SimulationSession reconnect_session;
    reconnect_session.connect_client(ClientRole::CommandConsole, 101U);
    reconnect_session.connect_client(ClientRole::TacticalViewer, 201U);
    reconnect_session.start_scenario();
    reconnect_session.disconnect_client(ClientRole::TacticalViewer, "viewer reconnect path");
    reconnect_session.connect_client(ClientRole::TacticalViewer, 201U);
    reconnect_session.request_track();
    const auto reconnect_frame = icss::view::render_tactical_frame(
        reconnect_session.latest_snapshot(),
        reconnect_session.events(),
        icss::view::make_replay_cursor(reconnect_session.events().size(),
                                       reconnect_session.events().empty() ? 0 : reconnect_session.events().size() - 1));
    assert(reconnect_frame.find("connection=reconnected") != std::string::npos);
    assert(reconnect_frame.find("freshness=resync") != std::string::npos);
    return 0;
}
