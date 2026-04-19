#include <cassert>

#include "icss/protocol/messages.hpp"

int main() {
    using namespace icss::protocol;

    static_assert(kTcpMessageKinds.size() == 14);
    static_assert(kUdpMessageKinds.size() == 5);
    static_assert(kEventTypes.size() == 11);

    assert(to_string(Transport::Tcp) == "tcp");
    assert(to_string(Transport::Udp) == "udp");
    assert(to_string(TcpMessageKind::CommandIssue) == "command_issue");
    assert(to_string(TcpMessageKind::ScenarioReset) == "scenario_reset");
    assert(to_string(TcpMessageKind::TrackRelease) == "track_release");
    assert(to_string(UdpMessageKind::Telemetry) == "telemetry");
    assert(to_string(TcpMessageKind::AarResponse) == "aar_response");
    assert(to_string(UdpMessageKind::ViewerHeartbeat) == "viewer_heartbeat");
    assert(to_string(EventType::JudgmentProduced) == "judgment_produced");

    return 0;
}
