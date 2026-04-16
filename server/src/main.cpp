#include <filesystem>
#include <iostream>

#include "icss/core/simulation.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const fs::path repo_root {ICSS_REPO_ROOT};
    auto session = run_baseline_demo();
    session.write_aar_artifacts(repo_root / "assets/sample-aar");
    session.write_example_output(repo_root / "examples/sample-output.md");

    const auto summary = session.build_summary();
    std::cout << "Interceptor Command Simulation System - server baseline (in-process authoritative runtime)\n";
    std::cout << "session_id=" << summary.session_id
              << ", phase=" << to_string(summary.phase)
              << ", snapshots=" << summary.snapshot_count
              << ", events=" << summary.event_count
              << ", resilience=" << summary.resilience_case << '\n';
    std::cout << "AAR artifacts written to " << (repo_root / "assets/sample-aar") << '\n';
    return summary.judgment_ready ? 0 : 1;
}
