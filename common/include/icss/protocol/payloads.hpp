#pragma once

#include <string>

#include "icss/protocol/messages.hpp"

namespace icss::protocol {

struct SessionCreatePayload {
    std::uint32_t requested_sender_id {};
    std::string scenario_name;
};

struct SessionJoinPayload {
    SessionEnvelope envelope {};
    std::string client_role;
};

struct SessionLeavePayload {
    SessionEnvelope envelope {};
    std::string reason;
};

struct ScenarioStartPayload {
    SessionEnvelope envelope {};
    std::string scenario_name;
    int world_width {2304};
    int world_height {1536};
    int target_start_x {480};
    int target_start_y {1200};
    int target_velocity_x {5};
    int target_velocity_y {-3};
    int interceptor_start_x {0};
    int interceptor_start_y {0};
    int interceptor_speed_per_tick {32};
    int intercept_radius {24};
    int engagement_timeout_ticks {60};
    int seeker_fov_deg {45};
    int launch_angle_deg {45};
};

struct ScenarioStopPayload {
    SessionEnvelope envelope {};
    std::string reason;
};

struct ScenarioResetPayload {
    SessionEnvelope envelope {};
    std::string reason;
};

struct TrackAcquirePayload {
    SessionEnvelope envelope {};
    std::string target_id;
};

struct TrackDropPayload {
    SessionEnvelope envelope {};
    std::string target_id;
};

struct InterceptorReadyPayload {
    SessionEnvelope envelope {};
    std::string interceptor_id;
};

struct EngageOrderPayload {
    SessionEnvelope envelope {};
    std::string interceptor_id;
    std::string target_id;
};

struct AssessmentPayload {
    SessionEnvelope envelope {};
    bool accepted {false};
    std::string outcome;
};

struct CommandAckPayload {
    SessionEnvelope envelope {};
    bool accepted {false};
    std::string reason;
};

struct AarResponsePayload {
    SessionEnvelope envelope {};
    std::uint64_t replay_cursor_index {};
    std::string control {"absolute"};
    std::uint64_t requested_index {};
    bool clamped {false};
    std::string assessment_code;
    std::string resilience_case;
    std::uint64_t total_events {};
    std::string event_type;
    std::string event_summary;
};

struct SnapshotPayload {
    SessionEnvelope envelope {};
    SnapshotHeader header {};
    std::string phase;
    int world_width {2304};
    int world_height {1536};
    std::string target_id;
    bool target_active {false};
    int target_x {0};
    int target_y {0};
    float target_world_x {0.0F};
    float target_world_y {0.0F};
    int target_velocity_x {0};
    int target_velocity_y {0};
    float target_velocity_world_x {0.0F};
    float target_velocity_world_y {0.0F};
    float target_heading_deg {0.0F};
    std::string interceptor_id;
    bool interceptor_active {false};
    int interceptor_x {0};
    int interceptor_y {0};
    float interceptor_world_x {0.0F};
    float interceptor_world_y {0.0F};
    float interceptor_velocity_world_x {0.0F};
    float interceptor_velocity_world_y {0.0F};
    float interceptor_heading_deg {0.0F};
    int interceptor_speed_per_tick {0};
    float interceptor_acceleration_per_tick {0.0F};
    int intercept_radius {0};
    int engagement_timeout_ticks {0};
    float seeker_fov_deg {0.0F};
    bool seeker_lock {false};
    float off_boresight_deg {0.0F};
    bool predicted_intercept_valid {false};
    float predicted_intercept_x {0.0F};
    float predicted_intercept_y {0.0F};
    float time_to_intercept_s {0.0F};
    bool track_active {false};
    float track_estimated_x {0.0F};
    float track_estimated_y {0.0F};
    float track_estimated_vx {0.0F};
    float track_estimated_vy {0.0F};
    bool track_measurement_valid {false};
    float track_measurement_x {0.0F};
    float track_measurement_y {0.0F};
    float track_measurement_residual_distance {0.0F};
    float track_covariance_trace {0.0F};
    int track_measurement_age_ticks {0};
    int track_missed_updates {0};
    std::string interceptor_status;
    std::string engage_order_status;
    bool assessment_ready {false};
    std::string assessment_code;
    float launch_angle_deg {45.0F};
};

struct TelemetryPayload {
    SessionEnvelope envelope {};
    TelemetrySample sample {};
    std::string connection_state;
    std::uint64_t event_tick {};
    std::string event_type;
    std::string event_summary;
};

struct DisplayHeartbeatPayload {
    SessionEnvelope envelope {};
    std::uint64_t heartbeat_id {};
};

struct AarRequestPayload {
    SessionEnvelope envelope {};
    std::uint64_t replay_cursor_index {};
    std::string control {"absolute"};
};

}  // namespace icss::protocol
