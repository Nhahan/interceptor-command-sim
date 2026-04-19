#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <vector>

#include "icss/core/config.hpp"
#include "icss/core/types.hpp"

namespace icss::core {

class SimulationSession {
public:
    explicit SimulationSession(std::uint32_t session_id = 1001,
                               int tick_rate_hz = 20,
                               int telemetry_interval_ms = 200,
                               ScenarioConfig scenario = {});

    void connect_client(ClientRole role, std::uint32_t sender_id);
    void disconnect_client(ClientRole role, std::string reason);
    void timeout_client(ClientRole role, std::string reason);

    CommandResult start_scenario();
    CommandResult reset_session(std::string reason);
    void configure_scenario(ScenarioConfig scenario);
    CommandResult request_track();
    CommandResult release_track();
    CommandResult activate_asset();
    CommandResult issue_command();
    CommandResult record_transport_rejection(std::string summary,
                                             std::string reason,
                                             std::vector<std::string> entity_ids = {});
    void advance_tick();
    void archive_session();

    [[nodiscard]] SessionPhase phase() const;
    [[nodiscard]] const std::vector<EventRecord>& events() const;
    [[nodiscard]] const std::vector<Snapshot>& snapshots() const;
    [[nodiscard]] Snapshot latest_snapshot() const;
    [[nodiscard]] SessionSummary build_summary() const;

    void write_aar_artifacts(const std::filesystem::path& output_dir) const;
    void write_example_output(const std::filesystem::path& output_file) const;

private:
    std::uint64_t next_timestamp_ms();
    ClientState& client(ClientRole role);
    const ClientState& client(ClientRole role) const;
    void push_event(protocol::EventType type,
                    std::string source,
                    std::vector<std::string> entity_ids,
                    std::string summary,
                    std::string details = {});
    void record_snapshot(float packet_loss_pct = 0.0F);
    void clear_track_state();
    void seed_track_state_from_target();
    void update_track_state();
    void reset_world_state_from_scenario();
    CommandResult reject_command(std::string summary,
                                 std::string reason,
                                 std::vector<std::string> entity_ids = {});

    std::uint32_t session_id_;
    SessionPhase phase_ {SessionPhase::Initialized};
    std::uint64_t tick_ {0};
    std::uint64_t sequence_ {0};
    std::uint64_t clock_ms_ {1'776'327'000'000ULL};
    int tick_rate_hz_ {20};
    int telemetry_interval_ms_ {200};
    ScenarioConfig scenario_;
    EntityState target_ {"target-alpha", {3, 13}, false};
    EntityState asset_ {"asset-interceptor", {10, 2}, false};
    ClientState command_console_ {ClientRole::CommandConsole, ConnectionState::Disconnected, 0, 0};
    ClientState tactical_viewer_ {ClientRole::TacticalViewer, ConnectionState::Disconnected, 0, 0};
    TrackState track_;
    AssetStatus asset_status_ {AssetStatus::Idle};
    CommandLifecycle command_status_ {CommandLifecycle::None};
    JudgmentState judgment_;
    bool reconnect_exercised_ {false};
    bool timeout_exercised_ {false};
    bool packet_gap_exercised_ {false};
    std::optional<std::uint64_t> engagement_started_tick_;
    Vec2f track_estimate_world_ {};
    Vec2f track_estimate_velocity_ {};
    Vec2f track_measurement_world_ {};
    Vec2f track_position_variance_ {};
    Vec2f track_velocity_variance_ {};
    bool track_measurement_valid_ {false};
    std::uint32_t track_measurement_age_ticks_ {0};
    std::uint32_t track_missed_updates_ {0};
    Vec2f target_world_ {};
    Vec2f asset_world_ {};
    Vec2f target_velocity_world_ {};
    Vec2f asset_velocity_world_ {};
    bool seeker_lock_ {false};
    float off_boresight_deg_ {0.0F};
    std::vector<EventRecord> events_;
    std::vector<Snapshot> snapshots_;
};

SimulationSession run_baseline_demo();

}  // namespace icss::core
