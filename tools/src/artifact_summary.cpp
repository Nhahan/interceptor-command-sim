#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "icss/core/artifact_readers.hpp"

namespace {

struct CliOptions {
    enum class Mode : std::uint8_t {
        RepoRoot,
        CompareRoots,
    };

    Mode mode {Mode::RepoRoot};
    std::filesystem::path repo_root {ICSS_REPO_ROOT};
    std::filesystem::path guided_root;
    std::filesystem::path straight_root;
    std::filesystem::path runtime_log_root;
};

[[noreturn]] void throw_usage() {
    throw std::runtime_error(
        "usage: icss_artifact_summary [--repo-root PATH] | [--guided-root PATH --straight-root PATH [--runtime-log-root PATH]]");
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options;
    std::optional<std::filesystem::path> repo_root;
    std::optional<std::filesystem::path> guided_root;
    std::optional<std::filesystem::path> straight_root;
    std::optional<std::filesystem::path> runtime_log_root;

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg {argv[index]};
        auto require_value = [&](std::string_view label) -> std::filesystem::path {
            if (index + 1 >= argc) {
                throw std::runtime_error("missing value for " + std::string(label));
            }
            return std::filesystem::path {argv[++index]};
        };
        if (arg == "--repo-root") {
            repo_root = require_value(arg);
            continue;
        }
        if (arg == "--guided-root") {
            guided_root = require_value(arg);
            continue;
        }
        if (arg == "--straight-root") {
            straight_root = require_value(arg);
            continue;
        }
        if (arg == "--runtime-log-root") {
            runtime_log_root = require_value(arg);
            continue;
        }
        throw_usage();
    }

    const bool repo_mode_requested = repo_root.has_value();
    const bool compare_mode_requested = guided_root.has_value() || straight_root.has_value() || runtime_log_root.has_value();
    if (repo_mode_requested && compare_mode_requested) {
        throw std::runtime_error("--repo-root cannot be combined with compare-root flags");
    }

    if (!compare_mode_requested) {
        if (repo_root.has_value()) {
            options.repo_root = *repo_root;
        }
        return options;
    }

    if (!guided_root.has_value() || !straight_root.has_value()) {
        throw std::runtime_error("--guided-root and --straight-root must be provided together");
    }

    options.mode = CliOptions::Mode::CompareRoots;
    options.guided_root = *guided_root;
    options.straight_root = *straight_root;
    options.runtime_log_root = runtime_log_root.value_or(*guided_root);
    return options;
}

bool straight_bundle_exists(const std::filesystem::path& repo_root) {
    const auto straight_root = repo_root / "assets/sample-aar/straight";
    const bool has_summary = std::filesystem::exists(straight_root / "session-summary.json");
    const bool has_timeline = std::filesystem::exists(straight_root / "replay-timeline.json");
    if (has_summary != has_timeline) {
        throw std::runtime_error("straight artifact bundle is incomplete");
    }
    return has_summary && has_timeline;
}

icss::core::SessionSummaryArtifact read_guided_summary_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_session_summary_json(repo_root / "assets/sample-aar/session-summary.json");
}

icss::core::ReplayTimelineArtifact read_guided_timeline_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_replay_timeline_json(repo_root / "assets/sample-aar/replay-timeline.json");
}

icss::core::SessionSummaryArtifact read_straight_summary_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_session_summary_json(repo_root / "assets/sample-aar/straight/session-summary.json");
}

icss::core::ReplayTimelineArtifact read_straight_timeline_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_replay_timeline_json(repo_root / "assets/sample-aar/straight/replay-timeline.json");
}

icss::core::SessionSummaryArtifact read_summary_from_runtime_root(const std::filesystem::path& root) {
    return icss::core::read_session_summary_json(root / "assets/sample-aar/session-summary.json");
}

icss::core::ReplayTimelineArtifact read_timeline_from_runtime_root(const std::filesystem::path& root) {
    return icss::core::read_replay_timeline_json(root / "assets/sample-aar/replay-timeline.json");
}

icss::core::RuntimeLogArtifact read_runtime_log_from_runtime_root(const std::filesystem::path& root) {
    return icss::core::read_runtime_log(root / "logs/session.log");
}

template <typename Summary, typename Timeline, typename Log>
void print_guided_block(const Summary& summary, const Timeline& timeline, const Log& runtime_log) {
    std::cout << "guided.summary.session_id=" << summary.session_id << '\n';
    std::cout << "guided.summary.judgment_code=" << summary.judgment_code << '\n';
    std::cout << "guided.summary.resilience_case=" << summary.resilience_case << '\n';
    std::cout << "guided.summary.latest_freshness=" << summary.latest_freshness << '\n';
    std::cout << "guided.timeline.event_count=" << timeline.event_count << '\n';
    std::cout << "guided.timeline.last_event_type=" << (timeline.events.empty() ? "none" : timeline.events.back().event_type) << '\n';
    std::cout << "guided.log.backend=" << runtime_log.backend << '\n';
    std::cout << "guided.log.event_record_count=" << runtime_log.event_record_count << '\n';
    std::cout << "guided.log.last_event_type=" << runtime_log.last_recorded_event_type << '\n';
}

template <typename Summary, typename Timeline>
void print_straight_compare_block(const Summary& guided_summary,
                                  const Summary& straight_summary,
                                  const Timeline& straight_timeline) {
    std::cout << "straight.available=true\n";
    std::cout << "straight.summary.session_id=" << straight_summary.session_id << '\n';
    std::cout << "straight.summary.judgment_code=" << straight_summary.judgment_code << '\n';
    std::cout << "straight.summary.resilience_case=" << straight_summary.resilience_case << '\n';
    std::cout << "straight.summary.latest_freshness=" << straight_summary.latest_freshness << '\n';
    std::cout << "straight.timeline.event_count=" << straight_timeline.event_count << '\n';
    std::cout << "straight.timeline.last_event_type=" << (straight_timeline.events.empty() ? "none" : straight_timeline.events.back().event_type) << '\n';
    std::cout << "compare.guided_judgment=" << guided_summary.judgment_code << '\n';
    std::cout << "compare.straight_judgment=" << straight_summary.judgment_code << '\n';
    std::cout << "compare.guided_launch_mode=" << guided_summary.launch_mode << '\n';
    std::cout << "compare.straight_launch_mode=" << straight_summary.launch_mode << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_args(argc, argv);
        std::cout << "ICSS Artifact Summary\n";

        if (options.mode == CliOptions::Mode::RepoRoot) {
            const auto guided_summary = read_guided_summary_from_repo(options.repo_root);
            const auto guided_timeline = read_guided_timeline_from_repo(options.repo_root);
            const auto runtime_log = read_runtime_log_from_runtime_root(options.repo_root);
            print_guided_block(guided_summary, guided_timeline, runtime_log);

            if (!straight_bundle_exists(options.repo_root)) {
                std::cout << "straight.available=false\n";
                return 0;
            }

            const auto straight_summary = read_straight_summary_from_repo(options.repo_root);
            const auto straight_timeline = read_straight_timeline_from_repo(options.repo_root);
            print_straight_compare_block(guided_summary, straight_summary, straight_timeline);
            return 0;
        }

        const auto guided_summary = read_summary_from_runtime_root(options.guided_root);
        const auto guided_timeline = read_timeline_from_runtime_root(options.guided_root);
        const auto runtime_log = read_runtime_log_from_runtime_root(options.runtime_log_root);
        const auto straight_summary = read_summary_from_runtime_root(options.straight_root);
        const auto straight_timeline = read_timeline_from_runtime_root(options.straight_root);

        print_guided_block(guided_summary, guided_timeline, runtime_log);
        print_straight_compare_block(guided_summary, straight_summary, straight_timeline);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
