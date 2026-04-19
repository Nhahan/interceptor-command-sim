#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "icss/core/types.hpp"

namespace icss::core::detail {

std::string escape_json(std::string_view input);
std::vector<EventRecord> recent_events_for_artifact(const std::vector<EventRecord>& events);
float clampf(float value, float lo, float hi);
void bounce_world_axis(float& position, float& velocity, float limit);
float length(Vec2f value);
Vec2f subtract(Vec2f lhs, Vec2f rhs);
Vec2f add(Vec2f lhs, Vec2f rhs);
Vec2f scale(Vec2f value, float scalar);
Vec2f normalize(Vec2f value);
float heading_deg(Vec2f value);
float normalize_angle_deg(float value);
float covariance_trace(Vec2f position_variance, Vec2f velocity_variance);
Vec2f deterministic_noise(std::uint64_t tick);
bool measurement_due(std::uint64_t tick);
bool measurement_available(std::uint64_t tick);
std::string build_resilience_case(bool reconnect_exercised, bool timeout_exercised, bool packet_gap_exercised);

}  // namespace icss::core::detail
