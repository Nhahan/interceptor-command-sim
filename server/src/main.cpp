#include <filesystem>
#include <iostream>

#include "icss/core/runtime.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const fs::path repo_root {ICSS_REPO_ROOT};
    BaselineRuntime runtime(default_runtime_config(repo_root));
    const auto result = runtime.run();
    const auto& summary = result.summary;
    std::cout << "Interceptor Command Simulation System - server baseline (in-process authoritative runtime)\n";
    std::cout << "session_id=" << summary.session_id
              << ", phase=" << to_string(summary.phase)
              << ", snapshots=" << summary.snapshot_count
              << ", events=" << summary.event_count
              << ", resilience=" << summary.resilience_case << '\n';
    std::cout << "scenario=" << result.config.scenario.name
              << ", tick_rate_hz=" << result.config.server.tick_rate_hz
              << ", telemetry_interval_ms=" << result.config.scenario.telemetry_interval_ms << '\n';
    std::cout << "AAR artifacts written to " << (repo_root / result.config.logging.aar_output_dir) << '\n';
    return summary.judgment_ready ? 0 : 1;
}
