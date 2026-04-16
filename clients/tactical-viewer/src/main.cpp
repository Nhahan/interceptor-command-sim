#include <iostream>
#include <vector>

#include "icss/core/simulation.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    using namespace icss::core;
    namespace view = icss::view;

    auto session = run_baseline_demo();
    const auto& events = session.events();
    const auto snapshot = session.latest_snapshot();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    std::vector<EventRecord> recent(events.begin() + static_cast<std::ptrdiff_t>(start), events.end());

    const icss::protocol::SnapshotPayload snapshot_payload {
        snapshot.envelope,
        snapshot.header,
        snapshot.target.id,
        snapshot.asset.id,
        snapshot.tracking_active,
        snapshot.asset_ready,
        snapshot.judgment_ready,
    };
    const icss::protocol::TelemetryPayload telemetry_payload {
        snapshot.envelope,
        snapshot.telemetry,
        to_string(snapshot.viewer_connection),
    };

    std::cout << view::render_tactical_frame(snapshot, recent, events.size() - 1);
    std::cout << "wire_preview.snapshot=" << icss::protocol::serialize(snapshot_payload) << '\n';
    std::cout << "wire_preview.telemetry=" << icss::protocol::serialize(telemetry_payload) << '\n';
    return 0;
}
