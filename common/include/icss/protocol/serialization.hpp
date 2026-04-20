#pragma once

#include <string>
#include <string_view>

#include "icss/protocol/payloads.hpp"

namespace icss::protocol {

std::string serialize(const SessionCreatePayload& payload);
std::string serialize(const SessionJoinPayload& payload);
std::string serialize(const SessionLeavePayload& payload);
std::string serialize(const ScenarioStartPayload& payload);
std::string serialize(const ScenarioStopPayload& payload);
std::string serialize(const ScenarioResetPayload& payload);
std::string serialize(const TrackAcquirePayload& payload);
std::string serialize(const TrackDropPayload& payload);
std::string serialize(const InterceptorReadyPayload& payload);
std::string serialize(const EngageOrderPayload& payload);
std::string serialize(const AssessmentPayload& payload);
std::string serialize(const CommandAckPayload& payload);
std::string serialize(const AarResponsePayload& payload);
std::string serialize(const SnapshotPayload& payload);
std::string serialize(const TelemetryPayload& payload);
std::string serialize(const DisplayHeartbeatPayload& payload);
std::string serialize(const DisplayHeartbeatAckPayload& payload);
std::string serialize(const AarRequestPayload& payload);

SessionCreatePayload parse_session_create(std::string_view wire);
SessionJoinPayload parse_session_join(std::string_view wire);
SessionLeavePayload parse_session_leave(std::string_view wire);
ScenarioStartPayload parse_scenario_start(std::string_view wire);
ScenarioStopPayload parse_scenario_stop(std::string_view wire);
ScenarioResetPayload parse_scenario_reset(std::string_view wire);
TrackAcquirePayload parse_track_acquire(std::string_view wire);
TrackDropPayload parse_track_drop(std::string_view wire);
InterceptorReadyPayload parse_interceptor_ready(std::string_view wire);
EngageOrderPayload parse_engage_order(std::string_view wire);
AssessmentPayload parse_assessment(std::string_view wire);
CommandAckPayload parse_command_ack(std::string_view wire);
AarResponsePayload parse_aar_response(std::string_view wire);
SnapshotPayload parse_snapshot(std::string_view wire);
TelemetryPayload parse_telemetry(std::string_view wire);
AarRequestPayload parse_aar_request(std::string_view wire);
DisplayHeartbeatPayload parse_display_heartbeat(std::string_view wire);
DisplayHeartbeatAckPayload parse_display_heartbeat_ack(std::string_view wire);

}  // namespace icss::protocol
