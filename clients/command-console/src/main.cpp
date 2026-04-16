#include <iostream>
#include <vector>

#include "icss/core/simulation.hpp"
#include "icss/protocol/messages.hpp"

int main() {
    using namespace icss::core;
    using namespace icss::protocol;

    SimulationSession session;
    session.connect_client(ClientRole::CommandConsole, 101U);

    const std::vector<std::pair<std::string_view, CommandResult>> commands {
        {to_string(TcpMessageKind::ScenarioStart), session.start_scenario()},
        {to_string(TcpMessageKind::TrackRequest), session.request_track()},
        {to_string(TcpMessageKind::AssetActivate), session.activate_asset()},
        {to_string(TcpMessageKind::CommandIssue), session.issue_command()},
    };

    std::cout << "Command console baseline\n";
    for (const auto& [label, result] : commands) {
        std::cout << label << ": " << (result.accepted ? "accepted" : "rejected")
                  << " | reason=" << result.reason << '\n';
    }
    return 0;
}
