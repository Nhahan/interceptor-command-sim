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
    assert(frame.find("packet_loss=") != std::string::npos);
    assert(frame.find("AAR:") != std::string::npos);
    assert(frame.find("cursor_index=") != std::string::npos);
    assert(frame.find("Entities:") != std::string::npos);
    return 0;
}
