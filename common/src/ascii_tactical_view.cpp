#include "icss/view/ascii_tactical_view.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace icss::view {
namespace {

constexpr int kWidth = 24;
constexpr int kHeight = 16;

std::vector<std::string> make_grid() {
    return std::vector<std::string>(kHeight, std::string(kWidth, '.'));
}

void place(std::vector<std::string>& grid, int x, int y, char glyph) {
    if (y >= 0 && y < static_cast<int>(grid.size()) && x >= 0 && x < static_cast<int>(grid[y].size())) {
        grid[y][x] = glyph;
    }
}

int scale_axis(int value, int world_limit, int grid_limit) {
    if (grid_limit <= 1 || world_limit <= 1) {
        return 0;
    }
    const auto clamped = std::clamp(value, 0, world_limit - 1);
    return static_cast<int>((static_cast<long long>(clamped) * (grid_limit - 1)) / (world_limit - 1));
}

std::string freshness_label_impl(const icss::core::Snapshot& snapshot) {
    switch (snapshot.viewer_connection) {
    case icss::core::ConnectionState::TimedOut:
    case icss::core::ConnectionState::Disconnected:
        return "stale";
    case icss::core::ConnectionState::Reconnected:
        return "resync";
    case icss::core::ConnectionState::Connected:
        if (snapshot.telemetry.packet_loss_pct > 0.0F) {
            return "degraded";
        }
        return "fresh";
    }
    return "unknown";
}

}  // namespace

std::string freshness_label(const icss::core::Snapshot& snapshot) {
    return freshness_label_impl(snapshot);
}

std::string render_tactical_frame(const icss::core::Snapshot& snapshot,
                                  const std::vector<icss::core::EventRecord>& recent_events,
                                  ReplayCursor cursor) {
    auto grid = make_grid();
    place(grid,
          scale_axis(snapshot.target.position.x, snapshot.world_width, kWidth),
          scale_axis(snapshot.target.position.y, snapshot.world_height, kHeight),
          'T');
    place(grid,
          scale_axis(snapshot.asset.position.x, snapshot.world_width, kWidth),
          scale_axis(snapshot.asset.position.y, snapshot.world_height, kHeight),
          'A');

    std::ostringstream out;
    out << "=== Tactical Viewer ===\n";
    for (const auto& row : grid) {
        out << row << '\n';
    }
    out << "Entities:\n";
    out << "- target=" << snapshot.target.id << " @ (" << snapshot.target.position.x << ", " << snapshot.target.position.y
        << ") active=" << (snapshot.target.active ? "yes" : "no") << '\n';
    out << "- interceptor=" << snapshot.asset.id << " @ (" << snapshot.asset.position.x << ", " << snapshot.asset.position.y
        << ") active=" << (snapshot.asset.active ? "yes" : "no") << '\n';
    out << "State:\n";
    out << "- phase=" << icss::core::to_string(snapshot.phase)
        << ", tracking=" << (snapshot.track.active ? "on" : "off")
        << " (confidence=" << snapshot.track.confidence_pct << "%)"
        << ", interceptor_status=" << icss::core::to_string(snapshot.asset_status)
        << ", command_status=" << icss::core::to_string(snapshot.command_status)
        << ", judgment=" << icss::core::to_string(snapshot.judgment.code) << '\n';
    out << "Telemetry:\n";
    out << "- connection=" << icss::core::to_string(snapshot.viewer_connection)
        << ", freshness=" << freshness_label_impl(snapshot)
        << ", snapshot_sequence=" << snapshot.header.snapshot_sequence
        << ", tick=" << snapshot.telemetry.tick
        << ", latency_ms=" << snapshot.telemetry.latency_ms
        << ", packet_loss_pct=" << std::fixed << std::setprecision(1) << snapshot.telemetry.packet_loss_pct
        << ", last_snapshot_ms=" << snapshot.telemetry.last_snapshot_timestamp_ms << '\n';
    out << "AAR:\n";
    out << "- cursor_index=" << cursor.index << "/" << cursor.total << "\n";
    out << "Recent events:\n";
    if (recent_events.empty()) {
        out << "- none\n";
        return out.str();
    }
    for (const auto& event : recent_events) {
        out << "- [tick " << event.header.tick << "] " << event.summary << " ("
            << icss::protocol::to_string(event.header.event_type) << ")\n";
    }
    return out.str();
}

}  // namespace icss::view
