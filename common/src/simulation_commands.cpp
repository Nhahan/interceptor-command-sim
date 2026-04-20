#include "icss/core/simulation.hpp"

#include <cmath>

namespace icss::core {

CommandResult SimulationSession::start_scenario() {
    if (fire_control_console_.connection == ConnectionState::Disconnected) {
        return reject_command("Scenario start rejected",
                              "fire control console must connect before starting the scenario");
    }
    if (phase_ != SessionPhase::Standby) {
        return reject_command("Scenario start rejected",
                              "scenario can only be started from standby state");
    }
    phase_ = SessionPhase::Detecting;
    target_.active = true;
    clear_track_state();
    interceptor_status_ = InterceptorStatus::Idle;
    engage_order_status_ = EngageOrderStatus::None;
    assessment_ = {};
    push_event(protocol::EventType::SessionStarted,
               "simulation_server",
               {target_.id},
               "Scenario started",
               "The authoritative loop is now in detecting state.");
    record_snapshot();
    return {true, "scenario started", protocol::TcpMessageKind::ScenarioStart};
}

void SimulationSession::configure_scenario(ScenarioConfig scenario) {
    scenario_ = std::move(scenario);
    reset_world_state_from_scenario();
    clear_track_state();
    interceptor_status_ = InterceptorStatus::Idle;
    engage_order_status_ = EngageOrderStatus::None;
    assessment_ = {};
    engagement_started_tick_.reset();
}

CommandResult SimulationSession::reset_session(std::string) {
    tick_ = 0;
    phase_ = SessionPhase::Standby;
    reset_world_state_from_scenario();
    clear_track_state();
    interceptor_status_ = InterceptorStatus::Idle;
    engage_order_status_ = EngageOrderStatus::None;
    assessment_ = {};
    engagement_started_tick_.reset();
    reconnect_exercised_ = false;
    timeout_exercised_ = false;
    packet_gap_exercised_ = false;
    events_.clear();
    snapshots_.clear();
    if (fire_control_console_.connection == ConnectionState::Reconnected) {
        fire_control_console_.connection = ConnectionState::Connected;
    }
    if (tactical_display_.connection == ConnectionState::Reconnected) {
        tactical_display_.connection = ConnectionState::Connected;
    }
    record_snapshot();
    return {true, "session reset to standby state", protocol::TcpMessageKind::CommandAck};
}

CommandResult SimulationSession::request_track() {
    if (phase_ == SessionPhase::Detecting) {
        seed_track_state_from_target();
        phase_ = SessionPhase::Tracking;
        push_event(protocol::EventType::TrackUpdated,
                   "fire_control_console",
                   {target_.id},
                   "Track acquired",
                   "A target track is now established for pre-launch fire control.");
        record_snapshot();
        return {true, "track accepted", protocol::TcpMessageKind::TrackAcquire};
    }
    if (phase_ == SessionPhase::InterceptorReady) {
        if (track_.active) {
            return reject_command("Track request rejected",
                                  "track is already established",
                                  {target_.id});
        }
        seed_track_state_from_target();
        push_event(protocol::EventType::TrackUpdated,
                   "fire_control_console",
                   {target_.id, asset_.id},
                   "Track acquired",
                   "Track was re-established while the interceptor remained ready.");
        record_snapshot();
        return {true, "track accepted", protocol::TcpMessageKind::TrackAcquire};
    }
    if (phase_ == SessionPhase::EngageOrdered || phase_ == SessionPhase::Intercepting) {
        return reject_command("Track request rejected",
                              "track control is unavailable after engage order",
                              {target_.id, asset_.id});
    }
    if (phase_ == SessionPhase::Assessed || phase_ == SessionPhase::Archived) {
        return reject_command("Track request rejected",
                              "track control is unavailable after archive",
                              {target_.id});
    }
    return reject_command("Track request rejected",
                          "track request is only valid while detecting or interceptor_ready",
                          {target_.id});
}

CommandResult SimulationSession::release_track() {
    if (phase_ == SessionPhase::Tracking) {
        if (!track_.active) {
            return reject_command("Track release rejected",
                                  "track is already dropped",
                                  {target_.id});
        }
        clear_track_state();
        phase_ = SessionPhase::Detecting;
        push_event(protocol::EventType::TrackUpdated,
                   "fire_control_console",
                   {target_.id},
                   "Track dropped",
                   "Unguided intercept will be used unless track is reacquired before engage order. Session returned to detecting.");
        record_snapshot();
        return {true, "track released", protocol::TcpMessageKind::TrackDrop};
    }
    if (phase_ == SessionPhase::InterceptorReady) {
        if (!track_.active) {
            return reject_command("Track release rejected",
                                  "track is already dropped",
                                  {target_.id, asset_.id});
        }
        clear_track_state();
        push_event(protocol::EventType::TrackUpdated,
                   "fire_control_console",
                   {target_.id, asset_.id},
                   "Track dropped",
                   "Unguided intercept will be used unless track is reacquired before engage order. The interceptor remains ready.");
        record_snapshot();
        return {true, "track released", protocol::TcpMessageKind::TrackDrop};
    }
    if (phase_ == SessionPhase::EngageOrdered || phase_ == SessionPhase::Intercepting) {
        return reject_command("Track release rejected",
                              "track control is unavailable after engage order",
                              {target_.id, asset_.id});
    }
    if (phase_ == SessionPhase::Assessed || phase_ == SessionPhase::Archived) {
        return reject_command("Track release rejected",
                              "track control is unavailable after archive",
                              {target_.id});
    }
    return reject_command("Track release rejected",
                          "track release is only valid while tracking or interceptor_ready",
                          {target_.id});
}

CommandResult SimulationSession::activate_asset() {
    if (phase_ != SessionPhase::Tracking) {
        return reject_command("Interceptor activation rejected",
                              "interceptor activation is only valid while tracking",
                              {asset_.id});
    }
    if (!track_.active) {
        return reject_command("Interceptor activation rejected",
                              "interceptor activation requires an active track",
                              {asset_.id});
    }
    interceptor_status_ = InterceptorStatus::Ready;
    asset_.active = true;
    phase_ = SessionPhase::InterceptorReady;
    push_event(protocol::EventType::InterceptorUpdated,
               "fire_control_console",
               {asset_.id},
               "Interceptor readied",
               "Interceptor is readied for an engage order.");
    record_snapshot();
    return {true, "interceptor ready", protocol::TcpMessageKind::InterceptorReady};
}

CommandResult SimulationSession::issue_command() {
    if (phase_ != SessionPhase::InterceptorReady) {
        return reject_command("Engage order rejected",
                              "engage order is only valid while interceptor_ready",
                              {asset_.id, target_.id});
    }
    if (interceptor_status_ != InterceptorStatus::Ready) {
        return reject_command("Engage order rejected",
                              "engage order requires interceptor_ready state",
                              {asset_.id, target_.id});
    }
    engage_order_status_ = EngageOrderStatus::Accepted;
    phase_ = SessionPhase::EngageOrdered;
    engagement_started_tick_ = tick_;
    const auto launch_angle_rad = static_cast<float>(scenario_.launch_angle_deg) * 3.1415926535F / 180.0F;
    const auto interceptor_speed = static_cast<float>(scenario_.interceptor_speed_per_tick);
    const bool tracked_intercept_launch = track_.active;
    interceptor_velocity_world_ = {
        std::cos(launch_angle_rad) * interceptor_speed,
        std::sin(launch_angle_rad) * interceptor_speed,
    };
    push_event(protocol::EventType::EngageOrderAccepted,
               "fire_control_console",
               {asset_.id, target_.id},
               "Launch accepted",
               "Authoritative server accepted the operator command and launched the interceptor on a "
                   + std::string(tracked_intercept_launch ? "tracked_intercept" : "unguided_intercept")
                   + " path at " + std::to_string(scenario_.launch_angle_deg) + " deg.");
    record_snapshot();
    return {true, "engage order accepted", protocol::TcpMessageKind::CommandAck};
}

CommandResult SimulationSession::record_transport_rejection(std::string summary,
                                                            std::string reason,
                                                            std::vector<std::string> entity_ids) {
    push_event(protocol::EventType::EngageOrderRejected,
               "simulation_server",
               std::move(entity_ids),
               std::move(summary),
               reason);
    return {false, std::move(reason), protocol::TcpMessageKind::CommandAck};
}

void SimulationSession::archive_session() {
    if (phase_ == SessionPhase::Archived) {
        return;
    }
    phase_ = SessionPhase::Archived;
    push_event(protocol::EventType::SessionEnded,
               "simulation_server",
               {target_.id, asset_.id},
               "Session archived",
               "Scenario completed and artifacts are ready for AAR.");
    record_snapshot();
}

}  // namespace icss::core
