#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "icss/net/transport.hpp"
#include "icss/protocol/frame_codec.hpp"

namespace icss::net::detail {

inline constexpr std::uint32_t kDefaultSessionId = 1001U;
inline constexpr std::string_view kTargetId = "target-alpha";
inline constexpr std::string_view kAssetId = "interceptor-alpha";
inline constexpr std::string_view kSampleOutputSchemaVersion = "icss-sample-output-v1";

icss::protocol::FrameFormat parse_frame_format(std::string_view value);
std::vector<icss::core::EventRecord> recent_events(const icss::core::SimulationSession& session);
const icss::core::EventRecord* latest_event_at_or_before_tick(const std::vector<icss::core::EventRecord>& events,
                                                              std::uint64_t tick);
icss::protocol::SnapshotPayload to_snapshot_payload(const icss::core::Snapshot& snapshot);
icss::protocol::TelemetryPayload to_telemetry_payload(const icss::core::Snapshot& snapshot,
                                                      const std::vector<icss::core::EventRecord>& events);

std::unique_ptr<TransportBackend> make_inprocess_transport(const icss::core::RuntimeConfig& config);
std::unique_ptr<TransportBackend> make_socket_live_transport(const icss::core::RuntimeConfig& config);

}  // namespace icss::net::detail
