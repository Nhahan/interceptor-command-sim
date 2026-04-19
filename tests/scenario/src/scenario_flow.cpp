#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "icss/core/simulation.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace std::string_literals;

    auto session = run_baseline_demo();
    const auto repo_config = load_runtime_config(std::filesystem::path {ICSS_REPO_ROOT});
    const auto summary = session.build_summary();

    assert(summary.judgment_ready);
    assert(summary.phase == SessionPhase::Archived);
    assert(summary.snapshot_count >= 5);
    assert(summary.event_count >= 8);
    assert(session.latest_snapshot().envelope.sender_id == kServerSenderId);
    assert(summary.judgment_code == JudgmentCode::InterceptSuccess);
    assert(session.latest_snapshot().asset_status == AssetStatus::Complete);
    assert(session.latest_snapshot().command_status == CommandLifecycle::Completed);
    assert(session.latest_snapshot().world_width == repo_config.scenario.world_width);
    assert(session.latest_snapshot().world_height == repo_config.scenario.world_height);
    assert(session.latest_snapshot().engagement_timeout_ticks == repo_config.scenario.engagement_timeout_ticks);
    assert(!session.latest_snapshot().target.active);
    assert(session.latest_snapshot().target_velocity.x == 0.0F);
    assert(session.latest_snapshot().target_velocity.y == 0.0F);
    assert(session.latest_snapshot().track.measurement_residual_distance >= 0.0F);
    assert(session.latest_snapshot().track.covariance_trace > 0.0F);
    bool observed_measurement_gap = false;
    bool observed_missed_update = false;
    bool observed_stale_but_valid_measurement = false;
    for (const auto& snapshot : session.snapshots()) {
        observed_measurement_gap = observed_measurement_gap || snapshot.track.measurement_age_ticks > 0;
        observed_missed_update = observed_missed_update || snapshot.track.missed_updates > 0;
        observed_stale_but_valid_measurement = observed_stale_but_valid_measurement
            || (snapshot.track.measurement_valid && snapshot.track.measurement_age_ticks > 0);
    }
    assert(observed_measurement_gap);
    assert(observed_missed_update);
    assert(observed_stale_but_valid_measurement);

    SimulationSession reset_probe {1001U, 20, 200, ScenarioConfig {}};
    reset_probe.connect_client(ClientRole::CommandConsole, 101U);
    const auto start_result = reset_probe.start_scenario();
    assert(start_result.accepted);
    const auto before_reset = reset_probe.latest_snapshot();
    const auto reset_result = reset_probe.reset_session("test reset");
    assert(reset_result.accepted);
    const auto after_reset = reset_probe.latest_snapshot();
    assert(after_reset.phase == SessionPhase::Initialized);
    assert(after_reset.header.tick == 0U);
    assert(after_reset.header.snapshot_sequence > before_reset.header.snapshot_sequence);
    assert(after_reset.header.timestamp_ms > before_reset.header.timestamp_ms);

    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path out_dir = fs::temp_directory_path() / ("icss_scenario_test_" + unique_suffix);
    session.write_aar_artifacts(out_dir);
    assert(fs::exists(out_dir / "replay-timeline.json"));
    assert(fs::exists(out_dir / "session-summary.md"));

    std::ifstream summary_file(out_dir / "session-summary.md");
    std::string contents((std::istreambuf_iterator<char>(summary_file)), std::istreambuf_iterator<char>());
    assert(contents.find("resilience_case: reconnect_and_resync") != std::string::npos);
    assert(contents.find("udp_snapshot_gap_convergence") != std::string::npos);

    fs::remove_all(out_dir);
    return 0;
}
