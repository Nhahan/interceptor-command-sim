#include "icss/core/simulation.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::core {
namespace {

std::string escape_json(std::string_view input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

}  // namespace

SimulationSession::SimulationSession(std::uint32_t session_id)
    : session_id_(session_id) {}

void SimulationSession::connect_client(ClientRole role, std::uint32_t sender_id) {
    auto& state = client(role);
    const auto reconnecting = state.has_connected_before;
    state.sender_id = sender_id;
    state.last_seen_tick = tick_;
    state.has_connected_before = true;
    state.connection = reconnecting ? ConnectionState::Reconnected : ConnectionState::Connected;
    if (reconnecting) {
        reconnect_exercised_ = true;
        push_event(protocol::EventType::ClientReconnected,
                   to_string(role),
                   {},
                   "Client reconnected",
                   "Client resumed control-plane visibility and should resync on the next snapshot.");
        push_event(protocol::EventType::ResilienceTriggered,
                   "simulation_server",
                   {},
                   "Reconnect/resync path exercised",
                   "The baseline resilience case uses reconnect + latest-snapshot convergence.");
    } else {
        push_event(protocol::EventType::ClientJoined,
                   to_string(role),
                   {},
                   "Client joined session",
                   "Client registered before scenario activity.");
    }
}

void SimulationSession::disconnect_client(ClientRole role, std::string reason) {
    auto& state = client(role);
    state.connection = ConnectionState::Disconnected;
    push_event(protocol::EventType::ClientLeft,
               to_string(role),
               {},
               "Client disconnected",
               std::move(reason));
}

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
    push_event(protocol::EventType::SessionStarted,
               "simulation_server",
               {target_.id},
               "Scenario started",
               "The authoritative loop is now in detecting state.");
    record_snapshot();
    return {true, "scenario started", protocol::TcpMessageKind::ScenarioStart};
}

CommandResult SimulationSession::request_track() {
    if (phase_ != SessionPhase::Detecting) {
        return reject_command("Track request rejected",
                              "track request is only valid while detecting",
                              {target_.id});
    }
    tracking_active_ = true;
    phase_ = SessionPhase::Tracking;
    push_event(protocol::EventType::TrackUpdated,
               "command_console",
               {target_.id},
               "Track request accepted",
               "Target is now actively tracked by the server.");
    record_snapshot();
    return {true, "track accepted", protocol::TcpMessageKind::TrackRequest};
}

CommandResult SimulationSession::activate_asset() {
    if (!tracking_active_) {
        return reject_command("Asset activation rejected",
                              "asset activation requires an active track",
                              {asset_.id});
    }
    asset_ready_ = true;
    asset_.active = true;
    phase_ = SessionPhase::AssetReady;
    push_event(protocol::EventType::AssetUpdated,
               "command_console",
               {asset_.id},
               "Asset activation accepted",
               "Interceptor asset is ready for command issue.");
    record_snapshot();
    return {true, "asset ready", protocol::TcpMessageKind::AssetActivate};
}

CommandResult SimulationSession::issue_command() {
    if (!asset_ready_) {
        return reject_command("Command rejected",
                              "command issue requires asset_ready state",
                              {asset_.id, target_.id});
    }
    command_issued_ = true;
    phase_ = SessionPhase::CommandIssued;
    push_event(protocol::EventType::CommandAccepted,
               "command_console",
               {asset_.id, target_.id},
               "Command accepted",
               "Authoritative server accepted the operator command for judgment.");
    record_snapshot();
    return {true, "command accepted", protocol::TcpMessageKind::CommandAck};
}

void SimulationSession::advance_tick() {
    ++tick_;

    if (target_.active) {
        target_.position.x += 1;
        target_.position.y -= (tick_ % 2 == 0 ? 1 : 0);
    }

    if (command_issued_ && phase_ == SessionPhase::CommandIssued) {
        phase_ = SessionPhase::Engaging;
    } else if (command_issued_ && phase_ == SessionPhase::Engaging) {
        judgment_ready_ = true;
        phase_ = SessionPhase::Judged;
        push_event(protocol::EventType::JudgmentProduced,
                   "simulation_server",
                   {asset_.id, target_.id},
                   "Judgment produced",
                   "Server-authoritative judgment marked the intercept attempt as successful.");
    }

    float packet_loss = 0.0F;
    if (tick_ == 2 && !packet_gap_exercised_) {
        packet_gap_exercised_ = true;
        packet_loss = 25.0F;
        push_event(protocol::EventType::ResilienceTriggered,
                   "simulation_server",
                   {target_.id},
                   "Snapshot gap exercised",
                   "Viewer should converge on the next valid snapshot rather than require retransmission.");
    }

    record_snapshot(packet_loss);
}

void SimulationSession::archive_session() {
    phase_ = SessionPhase::Archived;
    push_event(protocol::EventType::SessionEnded,
               "simulation_server",
               {target_.id, asset_.id},
               "Session archived",
               "Scenario completed and artifacts are ready for AAR review.");
    record_snapshot();
}

SessionPhase SimulationSession::phase() const {
    return phase_;
}

const std::vector<EventRecord>& SimulationSession::events() const {
    return events_;
}

const std::vector<Snapshot>& SimulationSession::snapshots() const {
    return snapshots_;
}

Snapshot SimulationSession::latest_snapshot() const {
    if (snapshots_.empty()) {
        throw std::runtime_error("no snapshots recorded yet");
    }
    return snapshots_.back();
}

SessionSummary SimulationSession::build_summary() const {
    std::string resilience_case = "none";
    if (reconnect_exercised_ && packet_gap_exercised_) {
        resilience_case = "reconnect_and_resync,udp_snapshot_gap_convergence";
    } else if (reconnect_exercised_) {
        resilience_case = "reconnect_and_resync";
    } else if (packet_gap_exercised_) {
        resilience_case = "udp_snapshot_gap_convergence";
    }
    return {session_id_, phase_, snapshots_.size(), events_.size(), judgment_ready_,
            std::move(resilience_case)};
}

void SimulationSession::write_aar_artifacts(const std::filesystem::path& output_dir) const {
    std::filesystem::create_directories(output_dir);

    std::ofstream timeline(output_dir / "replay-timeline.json");
    timeline << "{\n";
    timeline << "  \"session_id\": \"" << session_id_ << "\",\n";
    timeline << "  \"events\": [\n";
    for (std::size_t index = 0; index < events_.size(); ++index) {
        const auto& event = events_[index];
        timeline << "    {\n";
        timeline << "      \"timestamp_ms\": " << event.header.timestamp_ms << ",\n";
        timeline << "      \"tick\": " << event.header.tick << ",\n";
        timeline << "      \"event_type\": \"" << protocol::to_string(event.header.event_type) << "\",\n";
        timeline << "      \"source\": \"" << escape_json(event.source) << "\",\n";
        timeline << "      \"entity_ids\": [";
        for (std::size_t entity_index = 0; entity_index < event.entity_ids.size(); ++entity_index) {
            timeline << "\"" << escape_json(event.entity_ids[entity_index]) << "\"";
            if (entity_index + 1 != event.entity_ids.size()) {
                timeline << ", ";
            }
        }
        timeline << "],\n";
        timeline << "      \"summary\": \"" << escape_json(event.summary) << "\",\n";
        timeline << "      \"details\": \"" << escape_json(event.details) << "\"\n";
        timeline << "    }" << (index + 1 == events_.size() ? "\n" : ",\n");
    }
    timeline << "  ]\n";
    timeline << "}\n";

    const auto summary = build_summary();
    std::ofstream summary_file(output_dir / "session-summary.md");
    summary_file << "# Sample AAR Session Summary\n\n";
    summary_file << "- session_id: " << summary.session_id << "\n";
    summary_file << "- final_phase: " << to_string(summary.phase) << "\n";
    summary_file << "- snapshot_count: " << summary.snapshot_count << "\n";
    summary_file << "- event_count: " << summary.event_count << "\n";
    summary_file << "- judgment_ready: " << (summary.judgment_ready ? "true" : "false") << "\n";
    summary_file << "- resilience_case: " << summary.resilience_case << "\n";
}

void SimulationSession::write_example_output(const std::filesystem::path& output_file) const {
    std::filesystem::create_directories(output_file.parent_path());
    std::ofstream out(output_file);

    std::vector<EventRecord> recent_events;
    const auto start = events_.size() > 4 ? events_.size() - 4 : 0;
    for (std::size_t index = start; index < events_.size(); ++index) {
        recent_events.push_back(events_[index]);
    }
    out << "# Sample Output\n\n";
    out << "```text\n";
    out << view::render_tactical_frame(latest_snapshot(), recent_events, events_.size() - 1);
    out << "```\n";
}

std::uint64_t SimulationSession::now_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

ClientState& SimulationSession::client(ClientRole role) {
    return role == ClientRole::CommandConsole ? command_console_ : tactical_viewer_;
}

const ClientState& SimulationSession::client(ClientRole role) const {
    return role == ClientRole::CommandConsole ? command_console_ : tactical_viewer_;
}

void SimulationSession::push_event(protocol::EventType type,
                                   std::string source,
                                   std::vector<std::string> entity_ids,
                                   std::string summary,
                                   std::string details) {
    events_.push_back({{tick_, now_ms(), type}, std::move(source), std::move(entity_ids), std::move(summary), std::move(details)});
}

void SimulationSession::record_snapshot(float packet_loss_pct) {
    ++sequence_;
    const auto timestamp = now_ms();
    command_console_.last_seen_tick = tick_;
    tactical_viewer_.last_seen_tick = tick_;
    if (tactical_viewer_.connection == ConnectionState::Reconnected) {
        tactical_viewer_.connection = ConnectionState::Connected;
    }
    snapshots_.push_back({
        {session_id_, kServerSenderId, sequence_},
        {tick_, timestamp, sequence_},
        target_,
        asset_,
        tracking_active_,
        asset_ready_,
        command_issued_,
        judgment_ready_,
        tactical_viewer_.connection,
        {tick_, 12U + static_cast<std::uint32_t>(tick_), packet_loss_pct, timestamp},
    });
}

CommandResult SimulationSession::reject_command(std::string summary,
                                                std::string reason,
                                                std::vector<std::string> entity_ids) {
    push_event(protocol::EventType::CommandRejected,
               "simulation_server",
               std::move(entity_ids),
               std::move(summary),
               reason);
    return {false, std::move(reason), protocol::TcpMessageKind::CommandAck};
}

SimulationSession run_baseline_demo() {
    SimulationSession session;
    session.connect_client(ClientRole::CommandConsole, 101U);
    session.connect_client(ClientRole::TacticalViewer, 201U);
    session.start_scenario();
    session.request_track();
    session.advance_tick();
    session.activate_asset();
    session.disconnect_client(ClientRole::TacticalViewer, "viewer reconnect exercised for resilience evidence");
    session.connect_client(ClientRole::TacticalViewer, 201U);
    session.issue_command();
    session.advance_tick();
    session.advance_tick();
    session.archive_session();
    return session;
}

}  // namespace icss::core
