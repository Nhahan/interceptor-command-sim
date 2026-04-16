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
std::string serialize(const TrackRequestPayload& payload);
std::string serialize(const AssetActivatePayload& payload);
std::string serialize(const CommandIssuePayload& payload);
std::string serialize(const JudgmentPayload& payload);
std::string serialize(const CommandAckPayload& payload);
std::string serialize(const AarResponsePayload& payload);
std::string serialize(const SnapshotPayload& payload);
std::string serialize(const TelemetryPayload& payload);
std::string serialize(const ViewerHeartbeatPayload& payload);
std::string serialize(const AarRequestPayload& payload);

SessionCreatePayload parse_session_create(std::string_view wire);
SessionJoinPayload parse_session_join(std::string_view wire);
SessionLeavePayload parse_session_leave(std::string_view wire);
ScenarioStartPayload parse_scenario_start(std::string_view wire);
ScenarioStopPayload parse_scenario_stop(std::string_view wire);
TrackRequestPayload parse_track_request(std::string_view wire);
AssetActivatePayload parse_asset_activate(std::string_view wire);
CommandIssuePayload parse_command_issue(std::string_view wire);
JudgmentPayload parse_judgment(std::string_view wire);
CommandAckPayload parse_command_ack(std::string_view wire);
AarResponsePayload parse_aar_response(std::string_view wire);
SnapshotPayload parse_snapshot(std::string_view wire);
TelemetryPayload parse_telemetry(std::string_view wire);
AarRequestPayload parse_aar_request(std::string_view wire);
ViewerHeartbeatPayload parse_viewer_heartbeat(std::string_view wire);

}  // namespace icss::protocol
