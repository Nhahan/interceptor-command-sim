#include <algorithm>
#include <cassert>
#include <string>

#include "icss/core/simulation.hpp"
#include "icss/protocol/messages.hpp"

int main() {
    using namespace icss::core;
    using namespace icss::protocol;

    SimulationSession session;
    session.connect_client(ClientRole::CommandConsole, 101U);

    const auto invalid_command = session.issue_command();
    assert(!invalid_command.accepted);
    assert(invalid_command.ack_kind == TcpMessageKind::CommandAck);

    const auto invalid_asset = session.activate_asset();
    assert(!invalid_asset.accepted);

    const auto started = session.start_scenario();
    assert(started.accepted);

    const auto duplicate_start = session.start_scenario();
    assert(!duplicate_start.accepted);

    const auto track = session.request_track();
    assert(track.accepted);

    const auto asset = session.activate_asset();
    assert(asset.accepted);

    session.archive_session();

    const auto post_archive_track = session.request_track();
    assert(!post_archive_track.accepted);

    const auto post_archive_asset = session.activate_asset();
    assert(!post_archive_asset.accepted);

    const auto post_archive_command = session.issue_command();
    assert(!post_archive_command.accepted);

    const auto& pre_reset_events = session.events();
    const auto rejected_count = std::count_if(pre_reset_events.begin(), pre_reset_events.end(), [](const EventRecord& event) {
        return event.header.event_type == EventType::CommandRejected;
    });
    assert(rejected_count >= 4);

    const auto reset = session.reset_session("reset after archive");
    assert(reset.accepted);
    const auto restart = session.start_scenario();
    assert(restart.accepted);

    const auto& events = session.events();
    assert(events.size() >= 1);
    assert(events.back().header.event_type == EventType::SessionStarted);
    assert(session.phase() == SessionPhase::Detecting);
    return 0;
}
