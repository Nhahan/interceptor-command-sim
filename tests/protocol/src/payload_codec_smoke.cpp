#include <cassert>
#include <stdexcept>
#include <string>

#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"

int main() {
    using namespace icss::protocol;

    const SessionCreatePayload session_create {101U, "basic_intercept_training"};
    const auto session_wire = serialize(session_create);
    const auto parsed_session = parse_session_create(session_wire);
    assert(parsed_session.requested_sender_id == 101U);
    assert(parsed_session.scenario_name == "basic_intercept_training");

    const CommandIssuePayload command {
        {1001U, 101U, 7U},
        "asset-interceptor",
        "target-alpha",
    };
    const auto command_wire = serialize(command);
    const auto parsed_command = parse_command_issue(command_wire);
    assert(parsed_command.envelope.session_id == 1001U);
    assert(parsed_command.envelope.sender_id == 101U);
    assert(parsed_command.target_id == "target-alpha");
    assert(parsed_command.asset_id == "asset-interceptor");

    const TelemetryPayload telemetry {
        {1001U, 1U, 8U},
        {3U, 35U, 12.5F, 1'776'327'000'800ULL},
        "reconnected",
    };
    const auto telemetry_wire = serialize(telemetry);
    const auto parsed_telemetry = parse_telemetry(telemetry_wire);
    assert(parsed_telemetry.sample.tick == 3U);
    assert(parsed_telemetry.sample.latency_ms == 35U);
    assert(parsed_telemetry.sample.packet_loss_pct > 12.0F);
    assert(parsed_telemetry.connection_state == "reconnected");

    bool rejected_wrong_kind = false;
    try {
        static_cast<void>(parse_command_issue(session_wire));
    } catch (const std::runtime_error&) {
        rejected_wrong_kind = true;
    }
    assert(rejected_wrong_kind);

    return 0;
}
