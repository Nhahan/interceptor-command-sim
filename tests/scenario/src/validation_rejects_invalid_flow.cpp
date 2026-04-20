#include <algorithm>
#include <cassert>
#include <string>

#include "icss/core/simulation.hpp"
#include "icss/protocol/messages.hpp"

int main() {
    using namespace icss::core;
    using namespace icss::protocol;

    SimulationSession session;
    session.connect_client(ClientRole::FireControlConsole, 101U);

    const auto invalid_command = session.issue_command();
    assert(!invalid_command.accepted);
    assert(invalid_command.ack_kind == TcpMessageKind::CommandAck);

    const auto invalid_asset = session.activate_asset();
    assert(!invalid_asset.accepted);

    const auto started = session.start_scenario();
    assert(started.accepted);

    const auto duplicate_start = session.start_scenario();
    assert(!duplicate_start.accepted);

    const auto track = session.request_track();
    assert(track.accepted);
    const auto release_from_tracking = session.release_track();
    assert(release_from_tracking.accepted);
    assert(session.phase() == SessionPhase::Detecting);
    const auto retrack = session.request_track();
    assert(retrack.accepted);
    const auto engage_order_status_before_transport_reject = session.latest_snapshot().engage_order_status;
    const auto assessment_before_transport_reject = session.latest_snapshot().assessment.code;
    const auto transport_reject = session.record_transport_rejection("Viewer registration rejected", "second viewer blocked");
    assert(!transport_reject.accepted);
    assert(session.latest_snapshot().engage_order_status == engage_order_status_before_transport_reject);
    assert(session.latest_snapshot().assessment.code == assessment_before_transport_reject);

    const auto interceptor = session.activate_asset();
    assert(interceptor.accepted);
    const auto release_from_interceptor_ready = session.release_track();
    assert(release_from_interceptor_ready.accepted);
    assert(session.phase() == SessionPhase::InterceptorReady);
    assert(!session.latest_snapshot().track.active);
    const auto reenable_from_interceptor_ready = session.request_track();
    assert(reenable_from_interceptor_ready.accepted);
    assert(session.phase() == SessionPhase::InterceptorReady);

    const auto command = session.issue_command();
    assert(command.accepted);
    const auto post_command_track_drop = session.release_track();
    assert(!post_command_track_drop.accepted);

    session.archive_session();

    const auto post_archive_track = session.request_track();
    assert(!post_archive_track.accepted);

    const auto post_archive_asset = session.activate_asset();
    assert(!post_archive_asset.accepted);

    const auto post_archive_command = session.issue_command();
    assert(!post_archive_command.accepted);

    const auto& pre_reset_events = session.events();
    const auto pre_archive_event_count = pre_reset_events.size();
    const auto rejected_count = std::count_if(pre_reset_events.begin(), pre_reset_events.end(), [](const EventRecord& event) {
        return event.header.event_type == EventType::EngageOrderRejected;
    });
    assert(rejected_count >= 4);

    session.archive_session();
    assert(session.events().size() == pre_archive_event_count);

    const auto reset = session.reset_session("reset after archive");
    assert(reset.accepted);
    const auto restart = session.start_scenario();
    assert(restart.accepted);

    const auto& events = session.events();
    assert(events.size() >= 1);
    assert(events.back().header.event_type == EventType::SessionStarted);
    assert(session.phase() == SessionPhase::Detecting);
    return 0;
}
