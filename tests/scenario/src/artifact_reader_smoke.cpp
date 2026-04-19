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
                                   std::string_view expected_guidance,
                                   std::string_view expected_launch_mode) {
        BaselineRuntime runtime(default_runtime_config(root), mode);
        const auto result = runtime.run();
        assert(result.summary.judgment_ready);

        const auto summary = read_session_summary_json(root / "assets/sample-aar/session-summary.json");
        assert(summary.schema_version == "icss-session-summary-json-v1");
        assert(summary.session_id == 1001U);
        assert(summary.judgment_code == expected_judgment);
        assert(summary.guidance_state == expected_guidance);
        assert(summary.launch_mode == expected_launch_mode);
        assert(summary.launch_angle_deg == 45U);
        assert(summary.latest_freshness == "fresh");
        assert(!summary.recent_events.empty());

        const auto timeline = read_replay_timeline_json(root / "assets/sample-aar/replay-timeline.json");
        assert(timeline.schema_version == "icss-replay-timeline-v1");
        assert(timeline.event_count >= 1U);
        assert(timeline.guidance_state == expected_guidance);
        assert(timeline.launch_mode == expected_launch_mode);
        assert(timeline.launch_angle_deg == 45U);
        assert(!timeline.events.empty());
        assert(!timeline.events.back().event_type.empty());

        const auto runtime_log = read_runtime_log(root / "logs/session.log");
        assert(runtime_log.schema_version == "icss-runtime-log-v1");
        assert(runtime_log.backend == "in_process");
        assert(runtime_log.session_id == 1001U);
        assert(runtime_log.guidance_state == expected_guidance);
        assert(runtime_log.launch_mode == expected_launch_mode);
        assert(runtime_log.launch_angle_deg == 45U);
        assert(runtime_log.event_record_count >= 1U);
        assert(!runtime_log.last_recorded_event_type.empty());
    };

    const auto guided_root = icss::testsupport::make_temp_configured_repo("icss_artifact_reader_guided_");
    verify_sample(guided_root, SampleMode::Guided, "intercept_success", "on", "guided");
    fs::remove_all(guided_root);

    const auto straight_root = icss::testsupport::make_temp_configured_repo("icss_artifact_reader_straight_");
    verify_sample(straight_root, SampleMode::Straight, "timeout_observed", "off", "straight");
    fs::remove_all(straight_root);

    return 0;
}
