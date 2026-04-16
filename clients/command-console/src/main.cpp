#include <iostream>
#include <vector>

#include "icss/core/simulation.hpp"
#include "icss/protocol/messages.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"

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

    const CommandIssuePayload preview {
        {1001U, 101U, 4U},
        "asset-interceptor",
        "target-alpha",
    };
    const AarRequestPayload aar_preview {
        {1001U, 101U, 5U},
        11U,
    };
    std::cout << "wire_preview.command_issue=" << serialize(preview) << '\n';
    std::cout << "wire_preview.aar_request=" << serialize(aar_preview) << '\n';
    return 0;
}
