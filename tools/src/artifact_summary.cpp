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
    std::filesystem::path tracked_intercept_root;
    std::filesystem::path unguided_intercept_root;
    std::filesystem::path runtime_log_root;
};

[[noreturn]] void throw_usage() {
    throw std::runtime_error(
        "usage: icss_artifact_summary [--repo-root PATH] | [--tracked_intercept-root PATH --unguided_intercept-root PATH [--runtime-log-root PATH]]");
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options;
    std::optional<std::filesystem::path> repo_root;
    std::optional<std::filesystem::path> tracked_intercept_root;
    std::optional<std::filesystem::path> unguided_intercept_root;
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
        if (arg == "--tracked_intercept-root") {
            tracked_intercept_root = require_value(arg);
            continue;
        }
        if (arg == "--unguided_intercept-root") {
            unguided_intercept_root = require_value(arg);
            continue;
        }
        if (arg == "--runtime-log-root") {
            runtime_log_root = require_value(arg);
            continue;
        }
        throw_usage();
    }

    const bool repo_mode_requested = repo_root.has_value();
    const bool compare_mode_requested = tracked_intercept_root.has_value() || unguided_intercept_root.has_value() || runtime_log_root.has_value();
    if (repo_mode_requested && compare_mode_requested) {
        throw std::runtime_error("--repo-root cannot be combined with compare-root flags");
    }

    if (!compare_mode_requested) {
        if (repo_root.has_value()) {
            options.repo_root = *repo_root;
        }
        return options;
    }

    if (!tracked_intercept_root.has_value() || !unguided_intercept_root.has_value()) {
        throw std::runtime_error("--tracked_intercept-root and --unguided_intercept-root must be provided together");
    }

    options.mode = CliOptions::Mode::CompareRoots;
    options.tracked_intercept_root = *tracked_intercept_root;
    options.unguided_intercept_root = *unguided_intercept_root;
    options.runtime_log_root = runtime_log_root.value_or(*tracked_intercept_root);
    return options;
}

bool unguided_intercept_bundle_exists(const std::filesystem::path& repo_root) {
    const auto unguided_intercept_root = repo_root / "assets/sample-aar/unguided_intercept";
    const bool has_summary = std::filesystem::exists(unguided_intercept_root / "session-summary.json");
    const bool has_timeline = std::filesystem::exists(unguided_intercept_root / "replay-timeline.json");
    if (has_summary != has_timeline) {
        throw std::runtime_error("unguided_intercept artifact bundle is incomplete");
    }
    return has_summary && has_timeline;
}

icss::core::SessionSummaryArtifact read_tracked_intercept_summary_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_session_summary_json(repo_root / "assets/sample-aar/session-summary.json");
}

icss::core::ReplayTimelineArtifact read_tracked_intercept_timeline_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_replay_timeline_json(repo_root / "assets/sample-aar/replay-timeline.json");
}

icss::core::SessionSummaryArtifact read_unguided_intercept_summary_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_session_summary_json(repo_root / "assets/sample-aar/unguided_intercept/session-summary.json");
}

icss::core::ReplayTimelineArtifact read_unguided_intercept_timeline_from_repo(const std::filesystem::path& repo_root) {
    return icss::core::read_replay_timeline_json(repo_root / "assets/sample-aar/unguided_intercept/replay-timeline.json");
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
void print_tracked_intercept_block(const Summary& summary, const Timeline& timeline, const Log& runtime_log) {
    std::cout << "tracked_intercept.summary.session_id=" << summary.session_id << '\n';
    std::cout << "tracked_intercept.summary.assessment_code=" << summary.assessment_code << '\n';
    std::cout << "tracked_intercept.summary.resilience_case=" << summary.resilience_case << '\n';
    std::cout << "tracked_intercept.summary.latest_freshness=" << summary.latest_freshness << '\n';
    std::cout << "tracked_intercept.timeline.event_count=" << timeline.event_count << '\n';
    std::cout << "tracked_intercept.timeline.last_event_type=" << (timeline.events.empty() ? "none" : timeline.events.back().event_type) << '\n';
    std::cout << "tracked_intercept.log.backend=" << runtime_log.backend << '\n';
    std::cout << "tracked_intercept.log.event_record_count=" << runtime_log.event_record_count << '\n';
    std::cout << "tracked_intercept.log.last_event_type=" << runtime_log.last_recorded_event_type << '\n';
}

template <typename Summary, typename Timeline>
void print_unguided_intercept_compare_block(const Summary& tracked_intercept_summary,
                                  const Summary& unguided_intercept_summary,
                                  const Timeline& unguided_intercept_timeline) {
    std::cout << "unguided_intercept.available=true\n";
    std::cout << "unguided_intercept.summary.session_id=" << unguided_intercept_summary.session_id << '\n';
    std::cout << "unguided_intercept.summary.assessment_code=" << unguided_intercept_summary.assessment_code << '\n';
    std::cout << "unguided_intercept.summary.resilience_case=" << unguided_intercept_summary.resilience_case << '\n';
    std::cout << "unguided_intercept.summary.latest_freshness=" << unguided_intercept_summary.latest_freshness << '\n';
    std::cout << "unguided_intercept.timeline.event_count=" << unguided_intercept_timeline.event_count << '\n';
    std::cout << "unguided_intercept.timeline.last_event_type=" << (unguided_intercept_timeline.events.empty() ? "none" : unguided_intercept_timeline.events.back().event_type) << '\n';
    std::cout << "compare.tracked_intercept_judgment=" << tracked_intercept_summary.assessment_code << '\n';
    std::cout << "compare.unguided_intercept_judgment=" << unguided_intercept_summary.assessment_code << '\n';
    std::cout << "compare.tracked_intercept_intercept_profile=" << tracked_intercept_summary.intercept_profile << '\n';
    std::cout << "compare.unguided_intercept_intercept_profile=" << unguided_intercept_summary.intercept_profile << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_args(argc, argv);
        std::cout << "ICSS Artifact Summary\n";

        if (options.mode == CliOptions::Mode::RepoRoot) {
            const auto tracked_intercept_summary = read_tracked_intercept_summary_from_repo(options.repo_root);
            const auto tracked_intercept_timeline = read_tracked_intercept_timeline_from_repo(options.repo_root);
            const auto runtime_log = read_runtime_log_from_runtime_root(options.repo_root);
            print_tracked_intercept_block(tracked_intercept_summary, tracked_intercept_timeline, runtime_log);

            if (!unguided_intercept_bundle_exists(options.repo_root)) {
                std::cout << "unguided_intercept.available=false\n";
                return 0;
            }

            const auto unguided_intercept_summary = read_unguided_intercept_summary_from_repo(options.repo_root);
            const auto unguided_intercept_timeline = read_unguided_intercept_timeline_from_repo(options.repo_root);
            print_unguided_intercept_compare_block(tracked_intercept_summary, unguided_intercept_summary, unguided_intercept_timeline);
            return 0;
        }

        const auto tracked_intercept_summary = read_summary_from_runtime_root(options.tracked_intercept_root);
        const auto tracked_intercept_timeline = read_timeline_from_runtime_root(options.tracked_intercept_root);
        const auto runtime_log = read_runtime_log_from_runtime_root(options.runtime_log_root);
        const auto unguided_intercept_summary = read_summary_from_runtime_root(options.unguided_intercept_root);
        const auto unguided_intercept_timeline = read_timeline_from_runtime_root(options.unguided_intercept_root);

        print_tracked_intercept_block(tracked_intercept_summary, tracked_intercept_timeline, runtime_log);
        print_unguided_intercept_compare_block(tracked_intercept_summary, unguided_intercept_summary, unguided_intercept_timeline);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
