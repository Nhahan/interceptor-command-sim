#include <cassert>
#include <filesystem>

#include "icss/core/artifact_readers.hpp"
#include "icss/core/runtime.hpp"
#include "tests/support/temp_repo.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const auto temp_root = icss::testsupport::make_temp_configured_repo("icss_artifact_reader_");

    BaselineRuntime runtime(default_runtime_config(temp_root));
    const auto result = runtime.run();
    assert(result.summary.judgment_ready);

    const auto summary = read_session_summary_json(temp_root / "assets/sample-aar/session-summary.json");
    assert(summary.schema_version == "icss-session-summary-json-v1");
    assert(summary.session_id == 1001U);
    assert(summary.judgment_code == "intercept_success");
    assert(summary.latest_freshness == "fresh");
    assert(!summary.recent_events.empty());

    const auto timeline = read_replay_timeline_json(temp_root / "assets/sample-aar/replay-timeline.json");
    assert(timeline.schema_version == "icss-replay-timeline-v1");
    assert(timeline.event_count >= 1U);
    assert(!timeline.events.empty());
    assert(!timeline.events.back().event_type.empty());

    const auto runtime_log = read_runtime_log(temp_root / "logs/session.log");
    assert(runtime_log.schema_version == "icss-runtime-log-v1");
    assert(runtime_log.backend == "in_process");
    assert(runtime_log.session_id == 1001U);
    assert(runtime_log.event_record_count >= 1U);
    assert(!runtime_log.last_recorded_event_type.empty());

    fs::remove_all(temp_root);
    return 0;
}
