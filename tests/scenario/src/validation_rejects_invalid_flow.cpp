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

    const auto& events = session.events();
    const auto rejected_count = std::count_if(events.begin(), events.end(), [](const EventRecord& event) {
        return event.header.event_type == EventType::CommandRejected;
    });

    assert(rejected_count >= 2);
    return 0;
}
