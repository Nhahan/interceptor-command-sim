#include <filesystem>
#include <iostream>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    namespace view = icss::view;

    auto transport = make_transport(BackendKind::InProcess, default_runtime_config(fs::path{ICSS_REPO_ROOT}));
    transport->connect_client(ClientRole::CommandConsole, 101U);
    transport->connect_client(ClientRole::TacticalViewer, 201U);
    transport->start_scenario();
    transport->dispatch(icss::protocol::TrackRequestPayload{{1001U, 101U, 1U}, "target-alpha"});
    transport->advance_tick();
    transport->dispatch(icss::protocol::AssetActivatePayload{{1001U, 101U, 2U}, "asset-interceptor"});
    transport->dispatch(icss::protocol::CommandIssuePayload{{1001U, 101U, 3U}, "asset-interceptor", "target-alpha"});
    transport->advance_tick();
    transport->advance_tick();
    transport->archive_session();

    const auto events = transport->events();
    const auto snapshot = transport->latest_snapshot();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    std::vector<EventRecord> recent(events.begin() + static_cast<std::ptrdiff_t>(start), events.end());
    auto cursor = view::make_replay_cursor(events.size(), events.size() > 1 ? events.size() - 2 : 0);
    cursor = view::step_forward(cursor);

    const icss::protocol::SnapshotPayload snapshot_payload {
        snapshot.envelope,
        snapshot.header,
        snapshot.target.id,
        snapshot.asset.id,
        snapshot.track.active,
        snapshot.track.confidence_pct,
        to_string(snapshot.asset_status),
        to_string(snapshot.command_status),
        snapshot.judgment.ready,
        to_string(snapshot.judgment.code),
    };
    const icss::protocol::TelemetryPayload telemetry_payload {
        snapshot.envelope,
        snapshot.telemetry,
        to_string(snapshot.viewer_connection),
    };

    std::cout << "backend=" << transport->backend_name() << '\n';
    std::cout << view::render_tactical_frame(snapshot, recent, cursor);
    std::cout << "wire_preview.snapshot=" << icss::protocol::serialize(snapshot_payload) << '\n';
    std::cout << "wire_preview.telemetry=" << icss::protocol::serialize(telemetry_payload) << '\n';
    return 0;
}
