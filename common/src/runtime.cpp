#include "icss/core/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace icss::core {
namespace {

std::string escape_json(std::string_view input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        const auto uch = static_cast<unsigned char>(ch);
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (uch < 0x20U) {
                constexpr char kHex[] = "0123456789ABCDEF";
                escaped += "\\u00";
                escaped += kHex[(uch >> 4U) & 0x0FU];
                escaped += kHex[uch & 0x0FU];
            } else {
                escaped += ch;
            }
            break;
        }
    }
    return escaped;
}

}  // namespace

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
    log << "{\"record_type\":\"session_summary\",\"level\":\"" << escape_json(config_.logging.level)
        << "\",\"session_id\":" << summary.session_id
        << ",\"phase\":\"" << escape_json(to_string(summary.phase))
        << "\",\"snapshot_count\":" << summary.snapshot_count
        << ",\"event_count\":" << summary.event_count
        << ",\"resilience\":\"" << escape_json(summary.resilience_case) << "\"}\n";
    for (const auto& event : session.events()) {
        log << "{\"record_type\":\"event\""
            << ",\"tick\":" << event.header.tick
            << ",\"timestamp_ms\":" << event.header.timestamp_ms
            << ",\"event_type\":\"" << icss::protocol::to_string(event.header.event_type) << "\""
            << ",\"source\":\"" << escape_json(event.source) << "\""
            << ",\"summary\":\"" << escape_json(event.summary) << "\""
            << ",\"details\":\"" << escape_json(event.details) << "\""
            << ",\"entity_ids\":[";
        for (std::size_t index = 0; index < event.entity_ids.size(); ++index) {
            log << "\"" << escape_json(event.entity_ids[index]) << "\"";
            if (index + 1 != event.entity_ids.size()) {
                log << ",";
            }
        }
        log << "]}\n";
    }

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
