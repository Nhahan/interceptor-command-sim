#include "icss/core/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "icss/net/transport.hpp"

namespace icss::core {
namespace {

constexpr std::string_view kRuntimeLogSchemaVersion = "icss-runtime-log-v1";

bool event_guidance_active(const icss::core::EventRecord& event) {
    if (event.summary.find("disabled") != std::string::npos
        || event.details.find("disabled") != std::string::npos
        || event.summary.find("straight") != std::string::npos
        || event.details.find("straight") != std::string::npos) {
        return false;
    }
    return event.summary.find("enabled") != std::string::npos
        || event.details.find("enabled") != std::string::npos
        || event.summary.find("guided") != std::string::npos
        || event.details.find("guided") != std::string::npos;
}

bool runtime_guidance_active(const std::vector<icss::core::EventRecord>& events, const icss::core::Snapshot* snapshot) {
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
        if (it->header.event_type == icss::protocol::EventType::CommandAccepted
            || it->header.event_type == icss::protocol::EventType::TrackUpdated) {
            return event_guidance_active(*it);
        }
    }
    if (snapshot == nullptr) {
        return false;
    }
    return snapshot->track.active;
}

const char* guidance_state_label(bool guidance_active, const icss::core::Snapshot* snapshot) {
    if (!guidance_active && snapshot == nullptr) {
        return "unknown";
    }
    return guidance_active ? "on" : "off";
}

const char* launch_mode_label(bool guidance_active, const icss::core::Snapshot* snapshot) {
    if (!guidance_active && snapshot == nullptr) {
        return "unknown";
    }
    return guidance_active ? "guided" : "straight";
}

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

void drive_sample_transport_sequence(icss::net::TransportBackend& transport,
                                     const icss::core::RuntimeConfig& config,
                                     icss::core::SampleMode sample_mode) {
    transport.connect_client(icss::core::ClientRole::CommandConsole, 101U);
    transport.connect_client(icss::core::ClientRole::TacticalViewer, 201U);
    if (!transport.start_scenario().accepted) {
        throw std::runtime_error("baseline start_scenario rejected");
    }
    if (!transport.dispatch(icss::protocol::TrackRequestPayload{{1001U, 101U, 1U}, "target-alpha"}).accepted) {
        throw std::runtime_error("baseline track_request rejected");
    }
    transport.advance_tick();
    if (!transport.dispatch(icss::protocol::AssetActivatePayload{{1001U, 101U, 2U}, "asset-interceptor"}).accepted) {
        throw std::runtime_error("baseline asset_activate rejected");
    }
    if (sample_mode == icss::core::SampleMode::Straight) {
        if (!transport.dispatch(icss::protocol::TrackReleasePayload{{1001U, 101U, 3U}, "target-alpha"}).accepted) {
            throw std::runtime_error("baseline track_release rejected");
        }
    }
    transport.disconnect_client(icss::core::ClientRole::TacticalViewer, "viewer reconnect exercised for resilience evidence");
    transport.connect_client(icss::core::ClientRole::TacticalViewer, 201U);
    const std::uint64_t command_sequence = sample_mode == icss::core::SampleMode::Straight ? 4U : 3U;
    if (!transport.dispatch(icss::protocol::CommandIssuePayload{{1001U, 101U, command_sequence}, "asset-interceptor", "target-alpha"}).accepted) {
        throw std::runtime_error("baseline command_issue rejected");
    }
    for (int i = 0; i < config.scenario.engagement_timeout_ticks && !transport.latest_snapshot().judgment.ready; ++i) {
        transport.advance_tick();
    }
}

void drive_sample_session_sequence(icss::core::SimulationSession& session,
                                   const icss::core::RuntimeConfig& config,
                                   icss::core::SampleMode sample_mode) {
    session.connect_client(icss::core::ClientRole::CommandConsole, 101U);
    session.connect_client(icss::core::ClientRole::TacticalViewer, 201U);
    if (!session.start_scenario().accepted) {
        throw std::runtime_error("baseline start_scenario rejected");
    }
    if (!session.request_track().accepted) {
        throw std::runtime_error("baseline track_request rejected");
    }
    session.advance_tick();
    if (!session.activate_asset().accepted) {
        throw std::runtime_error("baseline asset_activate rejected");
    }
    if (sample_mode == icss::core::SampleMode::Straight) {
        if (!session.release_track().accepted) {
            throw std::runtime_error("baseline track_release rejected");
        }
    }
    session.disconnect_client(icss::core::ClientRole::TacticalViewer, "viewer reconnect exercised for resilience evidence");
    session.connect_client(icss::core::ClientRole::TacticalViewer, 201U);
    if (!session.issue_command().accepted) {
        throw std::runtime_error("baseline command_issue rejected");
    }
    for (int i = 0; i < config.scenario.engagement_timeout_ticks && !session.latest_snapshot().judgment.ready; ++i) {
        session.advance_tick();
    }
}

}  // namespace

BaselineRuntime::BaselineRuntime(RuntimeConfig config, SampleMode sample_mode)
    : config_(std::move(config)),
      sample_mode_(sample_mode) {}

const RuntimeConfig& BaselineRuntime::config() const {
    return config_;
}

SampleMode BaselineRuntime::sample_mode() const {
    return sample_mode_;
}

RuntimeResult BaselineRuntime::run() const {
    auto transport = icss::net::make_transport(icss::net::BackendKind::InProcess, config_);
    drive_sample_transport_sequence(*transport, config_, sample_mode_);
    transport->archive_session();

    const auto summary = transport->summary();
    const auto aar_dir = config_.repo_root / config_.logging.aar_output_dir;
    const auto example_output = config_.repo_root / "examples/sample-output.md";

    transport->write_aar_artifacts(aar_dir);
    transport->write_example_output(example_output, icss::view::make_replay_cursor(summary.event_count, summary.event_count == 0 ? 0 : summary.event_count - 1));

    const auto latest_snapshot = transport->latest_snapshot();
    write_runtime_session_log(config_, transport->backend_name(), summary, transport->events(), &latest_snapshot);

    return {config_, summary};
}

RuntimeConfig default_runtime_config(const std::filesystem::path& repo_root) {
    return load_runtime_config(repo_root);
}

SimulationSession run_baseline_demo(const RuntimeConfig& config, SampleMode sample_mode) {
    SimulationSession session(1001, config.server.tick_rate_hz, config.scenario.telemetry_interval_ms, config.scenario);
    drive_sample_session_sequence(session, config, sample_mode);
    session.archive_session();
    return session;
}

SimulationSession run_baseline_demo(SampleMode sample_mode) {
    return run_baseline_demo(default_runtime_config(std::filesystem::path {ICSS_REPO_ROOT}), sample_mode);
}

SimulationSession run_baseline_demo() {
    return run_baseline_demo(default_runtime_config(std::filesystem::path {ICSS_REPO_ROOT}), SampleMode::Guided);
}

void write_runtime_session_log(const RuntimeConfig& config,
                               std::string_view backend_name,
                               const SessionSummary& summary,
                               const std::vector<EventRecord>& events,
                               const Snapshot* latest_snapshot) {
    const auto log_file = config.repo_root / config.logging.file_path;
    const auto parent = log_file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    const bool guidance_active = runtime_guidance_active(events, latest_snapshot);
    std::ofstream log(log_file);
    log << "{\"schema_version\":\"" << kRuntimeLogSchemaVersion
        << "\",\"record_type\":\"session_summary\",\"level\":\"" << escape_json(config.logging.level)
        << "\",\"backend\":\"" << escape_json(backend_name)
        << "\",\"session_id\":" << summary.session_id
        << ",\"phase\":\"" << escape_json(to_string(summary.phase))
        << "\",\"snapshot_count\":" << summary.snapshot_count
        << ",\"event_count\":" << summary.event_count
        << ",\"command_console_connection\":\"" << escape_json(to_string(summary.command_console_connection)) << "\""
        << ",\"viewer_connection\":\"" << escape_json(to_string(summary.viewer_connection)) << "\""
        << ",\"judgment_code\":\"" << escape_json(to_string(summary.judgment_code)) << "\""
        << ",\"guidance_state\":\"" << guidance_state_label(guidance_active, latest_snapshot) << "\""
        << ",\"launch_mode\":\"" << launch_mode_label(guidance_active, latest_snapshot) << "\""
        << ",\"launch_angle_deg\":" << (latest_snapshot != nullptr ? static_cast<int>(latest_snapshot->launch_angle_deg) : config.scenario.launch_angle_deg)
        << ",\"last_event_type\":\"" << escape_json(summary.has_last_event ? std::string(icss::protocol::to_string(summary.last_event_type)) : std::string("none")) << "\""
        << ",\"resilience\":\"" << escape_json(summary.resilience_case) << "\"}\n";
    for (const auto& event : events) {
        log << "{\"schema_version\":\"" << kRuntimeLogSchemaVersion << "\",\"record_type\":\"event\""
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
        log << "]";
        if (event.header.event_type == icss::protocol::EventType::TrackUpdated
            || event.header.event_type == icss::protocol::EventType::CommandAccepted) {
            log << ",\"guidance_active\":" << (event_guidance_active(event) ? "true" : "false");
        }
        if (event.header.event_type == icss::protocol::EventType::CommandAccepted) {
            log << ",\"launch_mode\":\"" << launch_mode_label(event_guidance_active(event), latest_snapshot) << "\""
                << ",\"launch_angle_deg\":" << (latest_snapshot != nullptr ? static_cast<int>(latest_snapshot->launch_angle_deg) : config.scenario.launch_angle_deg);
        }
        log << "}\n";
    }
}

}  // namespace icss::core
