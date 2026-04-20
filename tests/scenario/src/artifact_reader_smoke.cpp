#include <cassert>
#include <filesystem>

#include "icss/core/artifact_readers.hpp"
#include "icss/core/runtime.hpp"
#include "tests/support/temp_repo.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const auto verify_sample = [&](const fs::path& root,
                                   SampleMode mode,
                                   std::string_view expected_judgment,
                                   std::string_view expected_track_state,
                                   std::string_view expected_intercept_profile) {
        BaselineRuntime runtime(default_runtime_config(root), mode);
        const auto result = runtime.run();
        assert(result.summary.assessment_ready);

        const auto summary = read_session_summary_json(root / "assets/sample-aar/session-summary.json");
        assert(summary.schema_version == "icss-session-summary-json-v1");
        assert(summary.session_id == 1001U);
        assert(summary.assessment_code == expected_judgment);
        assert(summary.effective_track_state == expected_track_state);
        assert(summary.intercept_profile == expected_intercept_profile);
        assert(summary.launch_angle_deg == 45U);
        assert(summary.latest_freshness == "fresh");
        assert(!summary.recent_events.empty());

        const auto timeline = read_replay_timeline_json(root / "assets/sample-aar/replay-timeline.json");
        assert(timeline.schema_version == "icss-replay-timeline-v1");
        assert(timeline.event_count >= 1U);
        assert(timeline.effective_track_state == expected_track_state);
        assert(timeline.intercept_profile == expected_intercept_profile);
        assert(timeline.launch_angle_deg == 45U);
        assert(!timeline.events.empty());
        assert(!timeline.events.back().event_type.empty());

        const auto runtime_log = read_runtime_log(root / "logs/session.log");
        assert(runtime_log.schema_version == "icss-runtime-log-v1");
        assert(runtime_log.backend == "in_process");
        assert(runtime_log.session_id == 1001U);
        assert(runtime_log.effective_track_state == expected_track_state);
        assert(runtime_log.intercept_profile == expected_intercept_profile);
        assert(runtime_log.launch_angle_deg == 45U);
        assert(runtime_log.event_record_count >= 1U);
        assert(!runtime_log.last_recorded_event_type.empty());
    };

    const auto tracked_intercept_root = icss::testsupport::make_temp_configured_repo("icss_artifact_reader_tracked_intercept_");
    verify_sample(tracked_intercept_root, SampleMode::TrackedIntercept, "intercept_success", "tracked", "tracked_intercept");
    fs::remove_all(tracked_intercept_root);

    const auto unguided_intercept_root = icss::testsupport::make_temp_configured_repo("icss_artifact_reader_unguided_intercept_");
    verify_sample(unguided_intercept_root, SampleMode::UnguidedIntercept, "timeout_observed", "untracked", "unguided_intercept");
    fs::remove_all(unguided_intercept_root);

    return 0;
}
