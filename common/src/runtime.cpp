#include "icss/core/runtime.hpp"

#include <filesystem>
#include <fstream>

namespace icss::core {

BaselineRuntime::BaselineRuntime(RuntimeConfig config)
    : config_(std::move(config)) {}

const RuntimeConfig& BaselineRuntime::config() const {
    return config_;
}

RuntimeResult BaselineRuntime::run() const {
    auto session = run_baseline_demo(config_);
    const auto summary = session.build_summary();
    const auto aar_dir = config_.repo_root / config_.logging.aar_output_dir;
    const auto example_output = config_.repo_root / "examples/sample-output.md";
    const auto log_file = config_.repo_root / config_.logging.file_path;

    session.write_aar_artifacts(aar_dir);
    session.write_example_output(example_output);

    std::filesystem::create_directories(log_file.parent_path());
    std::ofstream log(log_file);
    log << "level=" << config_.logging.level << '\n';
    log << "session_id=" << summary.session_id << '\n';
    log << "phase=" << to_string(summary.phase) << '\n';
    log << "resilience=" << summary.resilience_case << '\n';

    return {config_, summary};
}

RuntimeConfig default_runtime_config(const std::filesystem::path& repo_root) {
    return load_runtime_config(repo_root);
}

SimulationSession run_baseline_demo(const RuntimeConfig& config) {
    SimulationSession session(1001, config.server.tick_rate_hz, config.scenario.telemetry_interval_ms);
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
