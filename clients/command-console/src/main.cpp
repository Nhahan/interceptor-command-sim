#include <filesystem>
#include <iostream>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/messages.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    using namespace icss::protocol;

    auto transport = make_transport(BackendKind::InProcess, default_runtime_config(fs::path{ICSS_REPO_ROOT}));
    transport->connect_client(ClientRole::CommandConsole, 101U);

    const std::vector<std::pair<std::string_view, CommandResult>> commands {
        {to_string(TcpMessageKind::ScenarioStart), transport->start_scenario()},
        {to_string(TcpMessageKind::TrackRequest), transport->dispatch(TrackRequestPayload{{1001U, 101U, 1U}, "target-alpha"})},
        {to_string(TcpMessageKind::AssetActivate), transport->dispatch(AssetActivatePayload{{1001U, 101U, 2U}, "asset-interceptor"})},
        {to_string(TcpMessageKind::CommandIssue), transport->dispatch(CommandIssuePayload{{1001U, 101U, 3U}, "asset-interceptor", "target-alpha"})},
    };

    std::cout << "Command console baseline\n";
    std::cout << "backend=" << transport->backend_name() << '\n';
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
