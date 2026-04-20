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
    switch (snapshot.display_connection) {
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
    if (snapshot.target.active) {
        place(grid,
              scale_axis(snapshot.target.position.x, snapshot.world_width, kWidth),
              scale_axis(snapshot.target.position.y, snapshot.world_height, kHeight),
              'T');
    }
    if (snapshot.interceptor.active) {
        place(grid,
              scale_axis(snapshot.interceptor.position.x, snapshot.world_width, kWidth),
              scale_axis(snapshot.interceptor.position.y, snapshot.world_height, kHeight),
              'A');
    }

    bool track_active = snapshot.track.active;
    for (auto it = recent_events.rbegin(); it != recent_events.rend(); ++it) {
        if (it->header.event_type == icss::protocol::EventType::EngageOrderAccepted
            || it->header.event_type == icss::protocol::EventType::TrackUpdated) {
            track_active = !(it->summary.find("disabled") != std::string::npos
                || it->details.find("disabled") != std::string::npos
                || it->summary.find("unguided_intercept") != std::string::npos
                || it->details.find("unguided_intercept") != std::string::npos);
            break;
        }
    }

    std::ostringstream out;
    out << "=== Tactical Display ===\n";
    for (const auto& row : grid) {
        out << row << '\n';
    }
    out << "Entities:\n";
    out << "- target=" << snapshot.target.id << " @ (" << snapshot.target.position.x << ", " << snapshot.target.position.y
        << ") active=" << (snapshot.target.active ? "yes" : "no") << '\n';
    out << "- interceptor=" << snapshot.interceptor.id << " @ (" << snapshot.interceptor.position.x << ", " << snapshot.interceptor.position.y
        << ") active=" << (snapshot.interceptor.active ? "yes" : "no") << '\n';
    out << "State:\n";
    out << "- phase=" << icss::core::to_string(snapshot.phase)
        << ", track=" << (track_active ? "tracked" : "untracked")
        << ", tracker_residual="
        << (snapshot.track.measurement_valid ? std::to_string(snapshot.track.measurement_residual_distance) : std::string("n/a"))
        << ", tracker_covariance=" << std::fixed << std::setprecision(1) << snapshot.track.covariance_trace
        << ", measurement_age=" << snapshot.track.measurement_age_ticks
        << ", measurement_valid=" << (snapshot.track.measurement_valid ? "yes" : "no")
        << ", tracker_estimate=(" << snapshot.track.estimated_position.x << ", " << snapshot.track.estimated_position.y << ")"
        << ", measurement=(" << snapshot.track.measurement_position.x << ", " << snapshot.track.measurement_position.y << ")"
        << ", interceptor_status=" << icss::core::to_string(snapshot.interceptor_status)
        << ", engage_order_status=" << icss::core::to_string(snapshot.engage_order_status)
        << ", assessment=" << icss::core::to_string(snapshot.assessment.code) << '\n';
    out << "- target_heading_deg=" << std::fixed << std::setprecision(1) << snapshot.target_heading_deg
        << ", interceptor_heading_deg=" << snapshot.interceptor_heading_deg
        << ", launch_angle_deg=" << snapshot.launch_angle_deg
        << ", intercept_profile=" << (track_active ? "tracked_intercept" : "unguided_intercept")
        << ", tti_s=" << snapshot.time_to_intercept_s
        << ", predicted_intercept_valid=" << (snapshot.predicted_intercept_valid ? "yes" : "no") << '\n';
    out << "Telemetry:\n";
    out << "- connection=" << icss::core::to_string(snapshot.display_connection)
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
