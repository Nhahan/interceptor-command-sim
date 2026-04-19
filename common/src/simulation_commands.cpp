#include "icss/core/simulation.hpp"

#include <cmath>

namespace icss::core {

CommandResult SimulationSession::start_scenario() {
    if (command_console_.connection == ConnectionState::Disconnected) {
        return reject_command("Scenario start rejected",
                              "command console must connect before starting the scenario");
    }
    if (phase_ != SessionPhase::Initialized) {
        return reject_command("Scenario start rejected",
                              "scenario can only be started from initialized state");
    }
    phase_ = SessionPhase::Detecting;
    target_.active = true;
    clear_track_state();
    asset_status_ = AssetStatus::Idle;
    command_status_ = CommandLifecycle::None;
    judgment_ = {};
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
    asset_status_ = AssetStatus::Idle;
    command_status_ = CommandLifecycle::None;
    judgment_ = {};
    engagement_started_tick_.reset();
}

CommandResult SimulationSession::reset_session(std::string) {
    tick_ = 0;
    phase_ = SessionPhase::Initialized;
    reset_world_state_from_scenario();
    clear_track_state();
    asset_status_ = AssetStatus::Idle;
    command_status_ = CommandLifecycle::None;
    judgment_ = {};
    engagement_started_tick_.reset();
    reconnect_exercised_ = false;
    timeout_exercised_ = false;
    packet_gap_exercised_ = false;
    events_.clear();
    snapshots_.clear();
    if (command_console_.connection == ConnectionState::Reconnected) {
        command_console_.connection = ConnectionState::Connected;
    }
    if (tactical_viewer_.connection == ConnectionState::Reconnected) {
        tactical_viewer_.connection = ConnectionState::Connected;
    }
    record_snapshot();
    return {true, "session reset to initialized state", protocol::TcpMessageKind::CommandAck};
}

CommandResult SimulationSession::request_track() {
    if (phase_ == SessionPhase::Detecting) {
        seed_track_state_from_target();
        phase_ = SessionPhase::Tracking;
        push_event(protocol::EventType::TrackUpdated,
                   "command_console",
                   {target_.id},
                   "Guidance enabled",
                   "Target-follow guidance is now enabled before launch.");
        record_snapshot();
        return {true, "track accepted", protocol::TcpMessageKind::TrackRequest};
    }
    if (phase_ == SessionPhase::AssetReady) {
        if (track_.active) {
            return reject_command("Track request rejected",
                                  "track guidance is already enabled",
                                  {target_.id});
        }
        seed_track_state_from_target();
        push_event(protocol::EventType::TrackUpdated,
                   "command_console",
                   {target_.id, asset_.id},
                   "Guidance enabled",
                   "Target-follow guidance was re-enabled while the interceptor remained ready.");
        record_snapshot();
        return {true, "track accepted", protocol::TcpMessageKind::TrackRequest};
    }
    if (phase_ == SessionPhase::CommandIssued || phase_ == SessionPhase::Engaging) {
        return reject_command("Track request rejected",
                              "track control is unavailable after command issue",
                              {target_.id, asset_.id});
    }
    if (phase_ == SessionPhase::Judged || phase_ == SessionPhase::Archived) {
        return reject_command("Track request rejected",
                              "track control is unavailable after archive",
                              {target_.id});
    }
    return reject_command("Track request rejected",
                          "track request is only valid while detecting or asset_ready",
                          {target_.id});
}

CommandResult SimulationSession::release_track() {
    if (phase_ == SessionPhase::Tracking) {
        if (!track_.active) {
            return reject_command("Track release rejected",
                                  "track guidance is already disabled",
                                  {target_.id});
        }
        clear_track_state();
        phase_ = SessionPhase::Detecting;
        push_event(protocol::EventType::TrackUpdated,
                   "command_console",
                   {target_.id},
                   "Guidance disabled",
                   "Straight launch will be used unless guidance is re-enabled before command. Session returned to detecting.");
        record_snapshot();
        return {true, "track released", protocol::TcpMessageKind::TrackRelease};
    }
    if (phase_ == SessionPhase::AssetReady) {
        if (!track_.active) {
            return reject_command("Track release rejected",
                                  "track guidance is already disabled",
                                  {target_.id, asset_.id});
        }
        clear_track_state();
        push_event(protocol::EventType::TrackUpdated,
                   "command_console",
                   {target_.id, asset_.id},
                   "Guidance disabled",
                   "Straight launch will be used unless guidance is re-enabled before command. The interceptor remains ready.");
        record_snapshot();
        return {true, "track released", protocol::TcpMessageKind::TrackRelease};
    }
    if (phase_ == SessionPhase::CommandIssued || phase_ == SessionPhase::Engaging) {
        return reject_command("Track release rejected",
                              "track control is unavailable after command issue",
                              {target_.id, asset_.id});
    }
    if (phase_ == SessionPhase::Judged || phase_ == SessionPhase::Archived) {
        return reject_command("Track release rejected",
                              "track control is unavailable after archive",
                              {target_.id});
    }
    return reject_command("Track release rejected",
                          "track release is only valid while tracking or asset_ready",
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
    asset_status_ = AssetStatus::Ready;
    asset_.active = true;
    phase_ = SessionPhase::AssetReady;
    push_event(protocol::EventType::AssetUpdated,
               "command_console",
               {asset_.id},
               "Interceptor activation accepted",
               "Interceptor is ready for command issue.");
    record_snapshot();
    return {true, "interceptor ready", protocol::TcpMessageKind::AssetActivate};
}

CommandResult SimulationSession::issue_command() {
    if (phase_ != SessionPhase::AssetReady) {
        return reject_command("Command rejected",
                              "command issue is only valid while asset_ready",
                              {asset_.id, target_.id});
    }
    if (asset_status_ != AssetStatus::Ready) {
        return reject_command("Command rejected",
                              "command issue requires asset_ready state",
                              {asset_.id, target_.id});
    }
    command_status_ = CommandLifecycle::Accepted;
    phase_ = SessionPhase::CommandIssued;
    engagement_started_tick_ = tick_;
    const auto launch_angle_rad = static_cast<float>(scenario_.launch_angle_deg) * 3.1415926535F / 180.0F;
    const auto interceptor_speed = static_cast<float>(scenario_.interceptor_speed_per_tick);
    const bool guided_launch = track_.active;
    asset_velocity_world_ = {
        std::cos(launch_angle_rad) * interceptor_speed,
        std::sin(launch_angle_rad) * interceptor_speed,
    };
    push_event(protocol::EventType::CommandAccepted,
               "command_console",
               {asset_.id, target_.id},
               "Launch accepted",
               "Authoritative server accepted the operator command and launched the interceptor on a "
                   + std::string(guided_launch ? "guided" : "straight")
                   + " path at " + std::to_string(scenario_.launch_angle_deg) + " deg.");
    record_snapshot();
    return {true, "command accepted", protocol::TcpMessageKind::CommandAck};
}

CommandResult SimulationSession::record_transport_rejection(std::string summary,
                                                            std::string reason,
                                                            std::vector<std::string> entity_ids) {
    push_event(protocol::EventType::CommandRejected,
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
               "Scenario completed and artifacts are ready for AAR review.");
    record_snapshot();
}

}  // namespace icss::core
