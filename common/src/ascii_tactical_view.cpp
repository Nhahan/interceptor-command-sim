#include "icss/view/ascii_tactical_view.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace icss::view {
namespace {

constexpr int kWidth = 12;
constexpr int kHeight = 8;

std::vector<std::string> make_grid() {
    return std::vector<std::string>(kHeight, std::string(kWidth, '.'));
}

void place(std::vector<std::string>& grid, int x, int y, char glyph) {
    if (y >= 0 && y < static_cast<int>(grid.size()) && x >= 0 && x < static_cast<int>(grid[y].size())) {
        grid[y][x] = glyph;
    }
}

}  // namespace

std::string render_tactical_frame(const icss::core::Snapshot& snapshot,
                                  const std::vector<icss::core::EventRecord>& recent_events,
                                  std::size_t aar_cursor_index) {
    auto grid = make_grid();
    place(grid, snapshot.target.position.x, snapshot.target.position.y, 'T');
    place(grid, snapshot.asset.position.x, snapshot.asset.position.y, 'A');

    std::ostringstream out;
    out << "=== Tactical Viewer ===\n";
    for (const auto& row : grid) {
        out << row << '\n';
    }
    out << "phase: tracking=" << (snapshot.tracking_active ? "on" : "off")
        << ", asset_ready=" << (snapshot.asset_ready ? "yes" : "no")
        << ", judgment_ready=" << (snapshot.judgment_ready ? "yes" : "no") << '\n';
    out << "connection=" << icss::core::to_string(snapshot.viewer_connection)
        << ", tick=" << snapshot.telemetry.tick
        << ", latency_ms=" << snapshot.telemetry.latency_ms
        << ", packet_loss=" << snapshot.telemetry.packet_loss_pct
        << ", last_snapshot_ms=" << snapshot.telemetry.last_snapshot_timestamp_ms << '\n';
    out << "AAR cursor index=" << aar_cursor_index << "\n";
    out << "Recent events:\n";
    for (const auto& event : recent_events) {
        out << "- [tick " << event.header.tick << "] " << event.summary << " ("
            << icss::protocol::to_string(event.header.event_type) << ")\n";
    }
    return out.str();
}

}  // namespace icss::view
