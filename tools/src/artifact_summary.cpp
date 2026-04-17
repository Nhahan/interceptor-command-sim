#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "icss/core/artifact_readers.hpp"

namespace {

std::filesystem::path repo_root_from_args(int argc, char** argv) {
    if (argc == 1) {
        return std::filesystem::path{ICSS_REPO_ROOT};
    }
    if (argc == 3 && std::string_view(argv[1]) == "--repo-root") {
        return std::filesystem::path{argv[2]};
    }
    throw std::runtime_error("usage: icss_artifact_summary [--repo-root PATH]");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto repo_root = repo_root_from_args(argc, argv);
        const auto summary = icss::core::read_session_summary_json(repo_root / "assets/sample-aar/session-summary.json");
        const auto timeline = icss::core::read_replay_timeline_json(repo_root / "assets/sample-aar/replay-timeline.json");
        const auto runtime_log = icss::core::read_runtime_log(repo_root / "logs/session.log");

        std::cout << "ICSS Artifact Summary\n";
        std::cout << "summary.session_id=" << summary.session_id << '\n';
        std::cout << "summary.judgment_code=" << summary.judgment_code << '\n';
        std::cout << "summary.resilience_case=" << summary.resilience_case << '\n';
        std::cout << "summary.latest_freshness=" << summary.latest_freshness << '\n';
        std::cout << "timeline.event_count=" << timeline.event_count << '\n';
        std::cout << "timeline.last_event_type=" << (timeline.events.empty() ? "none" : timeline.events.back().event_type) << '\n';
        std::cout << "log.backend=" << runtime_log.backend << '\n';
        std::cout << "log.event_record_count=" << runtime_log.event_record_count << '\n';
        std::cout << "log.last_event_type=" << runtime_log.last_recorded_event_type << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
