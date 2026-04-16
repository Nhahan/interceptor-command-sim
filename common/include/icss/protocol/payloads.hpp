#pragma once

#include <string>

#include "icss/protocol/messages.hpp"

namespace icss::protocol {

struct SessionCreatePayload {
    std::uint32_t requested_sender_id {};
    std::string scenario_name;
};

struct TrackRequestPayload {
    SessionEnvelope envelope {};
    std::string target_id;
};

struct AssetActivatePayload {
    SessionEnvelope envelope {};
    std::string asset_id;
};

struct CommandIssuePayload {
    SessionEnvelope envelope {};
    std::string asset_id;
    std::string target_id;
};

struct JudgmentPayload {
    SessionEnvelope envelope {};
    bool accepted {false};
    std::string outcome;
};

struct SnapshotPayload {
    SessionEnvelope envelope {};
    SnapshotHeader header {};
    std::string target_id;
    std::string asset_id;
    bool tracking_active {false};
    bool asset_ready {false};
    bool judgment_ready {false};
};

struct TelemetryPayload {
    SessionEnvelope envelope {};
    TelemetrySample sample {};
    std::string connection_state;
};

struct AarRequestPayload {
    SessionEnvelope envelope {};
    std::uint64_t replay_cursor_index {};
};

}  // namespace icss::protocol
