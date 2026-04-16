#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "icss/core/simulation.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace std::string_literals;

    auto session = run_baseline_demo();
    const auto summary = session.build_summary();

    assert(summary.judgment_ready);
    assert(summary.phase == SessionPhase::Archived);
    assert(summary.snapshot_count >= 5);
    assert(summary.event_count >= 8);
    assert(session.latest_snapshot().envelope.sender_id == kServerSenderId);

    const fs::path out_dir = fs::temp_directory_path() / "icss_scenario_test";
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
