#include "icss/core/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

#include "simulation_internal.hpp"

namespace icss::core::detail {

std::string escape_json(std::string_view input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::vector<EventRecord> recent_events_for_artifact(const std::vector<EventRecord>& events) {
    std::vector<EventRecord> recent;
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    for (std::size_t index = start; index < events.size(); ++index) {
        recent.push_back(events[index]);
    }
    return recent;
}

float clampf(float value, float lo, float hi) {
    return std::max(lo, std::min(value, hi));
}

void bounce_world_axis(float& position, float& velocity, float limit) {
    if (limit <= 1.0F || velocity == 0.0F) {
        position = clampf(position, 0.0F, std::max(0.0F, limit - 1.0F));
        return;
    }
    position += velocity;
    if (position < 0.0F) {
        position = -position;
        velocity *= -1.0F;
    } else if (position > limit - 1.0F) {
        position = (limit - 1.0F) - (position - (limit - 1.0F));
        velocity *= -1.0F;
    }
    position = clampf(position, 0.0F, limit - 1.0F);
}

float length(Vec2f value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec2f subtract(Vec2f lhs, Vec2f rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

Vec2f add(Vec2f lhs, Vec2f rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y};
}

Vec2f scale(Vec2f value, float scalar) {
    return {value.x * scalar, value.y * scalar};
}

Vec2f normalize(Vec2f value) {
    const auto magnitude = length(value);
    if (magnitude <= 1e-5F) {
        return {0.0F, 0.0F};
    }
    return {value.x / magnitude, value.y / magnitude};
}

float heading_deg(Vec2f value) {
    if (std::abs(value.x) <= 1e-5F && std::abs(value.y) <= 1e-5F) {
        return 0.0F;
    }
    return std::atan2(value.y, value.x) * 180.0F / 3.1415926535F;
}

float normalize_angle_deg(float value) {
    while (value > 180.0F) value -= 360.0F;
    while (value < -180.0F) value += 360.0F;
    return value;
}

float covariance_trace(Vec2f position_variance, Vec2f velocity_variance) {
    return position_variance.x + position_variance.y + velocity_variance.x + velocity_variance.y;
}

Vec2f deterministic_noise(std::uint64_t tick) {
    const auto phase = static_cast<float>(tick);
    return {
        std::sin((phase * 0.63F) + 0.4F) * 3.5F,
        std::cos((phase * 0.47F) + 0.9F) * 3.0F,
    };
}

bool measurement_due(std::uint64_t tick) {
    return (tick % 3U) == 0U;
}

bool measurement_available(std::uint64_t tick) {
    if (!measurement_due(tick)) {
        return false;
    }
    const auto measurement_index = tick / 3U;
    return (measurement_index % 5U) != 3U;
}

std::string build_resilience_case(bool reconnect_exercised, bool timeout_exercised, bool packet_gap_exercised) {
    if (reconnect_exercised && packet_gap_exercised && timeout_exercised) {
        return "reconnect_and_resync,udp_snapshot_gap_convergence,timeout_visibility";
    }
    if (reconnect_exercised && packet_gap_exercised) {
        return "reconnect_and_resync,udp_snapshot_gap_convergence";
    }
    if (reconnect_exercised && timeout_exercised) {
        return "reconnect_and_resync,timeout_visibility";
    }
    if (packet_gap_exercised && timeout_exercised) {
        return "udp_snapshot_gap_convergence,timeout_visibility";
    }
    if (reconnect_exercised) {
        return "reconnect_and_resync";
    }
    if (timeout_exercised) {
        return "timeout_visibility";
    }
    if (packet_gap_exercised) {
        return "udp_snapshot_gap_convergence";
    }
    return "none";
}

}  // namespace icss::core::detail
