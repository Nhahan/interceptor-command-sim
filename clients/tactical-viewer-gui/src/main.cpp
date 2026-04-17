#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "icss/core/config.hpp"
#include "icss/core/types.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

struct ViewerOptions {
    std::string host {"127.0.0.1"};
    std::uint16_t udp_port {4001};
    std::uint16_t tcp_port {4000};
    std::uint32_t session_id {1001U};
    std::uint32_t sender_id {201U};
    std::uint64_t duration_ms {0};
    std::uint64_t heartbeat_interval_ms {1000};
    int width {1360};
    int height {860};
    bool hidden {false};
    bool headless {false};
    bool auto_control_script {false};
    std::string tcp_frame_format {"json"};
    std::vector<std::string> auto_controls;
    std::filesystem::path dump_state_path;
    std::filesystem::path font_path;
};

struct ControlState {
    bool connected {false};
    bool last_ok {false};
    std::string last_label {"idle"};
    std::string last_message {"control panel idle"};
    std::uint64_t sequence {1};
    std::size_t auto_step {0};
    std::uint64_t auto_last_action_ms {0};
};

struct ReviewState {
    bool available {false};
    bool loaded {false};
    bool visible {false};
    std::uint64_t cursor_index {0};
    std::uint64_t total_events {0};
    std::string judgment_code {"pending"};
    std::string resilience_case {"none"};
    std::string event_type {"none"};
    std::string event_summary {"review not requested"};
};

struct ViewerState {
    icss::core::Snapshot snapshot {};
    icss::core::ScenarioConfig planned_scenario {};
    bool received_snapshot {false};
    bool received_telemetry {false};
    std::deque<std::string> recent_server_events;
    std::uint64_t heartbeat_id {0};
    std::uint64_t heartbeat_count {0};
    std::uint64_t snapshot_count_received {0};
    std::uint64_t telemetry_count_received {0};
    std::uint64_t last_server_event_tick {0};
    std::string last_server_event_type {"none"};
    std::string last_server_event_summary {"no server event"};
    std::deque<icss::core::Vec2> target_history;
    std::deque<icss::core::Vec2> asset_history;
    bool command_visual_active {false};
    ControlState control;
    ReviewState review;
    std::string profile_label {"Balanced"};
};

struct Button {
    std::string label;
    SDL_Rect rect {};
};

struct GuiLayout {
    SDL_Rect header_panel {};
    SDL_Rect phase_strip {};
    SDL_Rect map_rect {};
    SDL_Rect entity_panel {};
    int panel_x {0};
    SDL_Rect phase_panel {};
    SDL_Rect decision_panel {};
    SDL_Rect resilience_panel {};
    SDL_Rect control_panel {};
    SDL_Rect event_panel {};
    std::vector<Button> buttons;
};

void push_timeline_entry(ViewerState& state, std::string message) {
    if (!state.recent_server_events.empty() && state.recent_server_events.front() == message) {
        return;
    }
    state.recent_server_events.push_front(std::move(message));
    while (state.recent_server_events.size() > 16) {
        state.recent_server_events.pop_back();
    }
}

std::string escape_json(std::string_view input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::filesystem::path default_font_path() {
    const std::vector<std::filesystem::path> candidates {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("no usable monospace font found; pass --font PATH");
}

std::vector<std::string> split_controls(const std::string& value) {
    std::vector<std::string> items;
    std::string current;
    for (char ch : value) {
        if (ch == ',') {
            if (!current.empty()) {
                items.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        items.push_back(current);
    }
    return items;
}

ViewerOptions parse_args(int argc, char** argv) {
    ViewerOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto require_value = [&](std::string_view label) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + std::string(label));
            }
            return argv[++i];
        };
        if (arg == "--host") {
            options.host = require_value(arg);
            continue;
        }
        if (arg == "--udp-port") {
            options.udp_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--tcp-port") {
            options.tcp_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--session-id") {
            options.session_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--sender-id") {
            options.sender_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--duration-ms") {
            options.duration_ms = std::stoull(require_value(arg));
            continue;
        }
        if (arg == "--heartbeat-interval-ms") {
            options.heartbeat_interval_ms = std::stoull(require_value(arg));
            continue;
        }
        if (arg == "--tcp-frame-format") {
            options.tcp_frame_format = require_value(arg);
            continue;
        }
        if (arg == "--auto-controls") {
            options.auto_controls = split_controls(require_value(arg));
            options.auto_control_script = true;
            continue;
        }
        if (arg == "--width") {
            options.width = std::stoi(require_value(arg));
            continue;
        }
        if (arg == "--height") {
            options.height = std::stoi(require_value(arg));
            continue;
        }
        if (arg == "--dump-state") {
            options.dump_state_path = require_value(arg);
            continue;
        }
        if (arg == "--font") {
            options.font_path = require_value(arg);
            continue;
        }
        if (arg == "--hidden") {
            options.hidden = true;
            continue;
        }
        if (arg == "--headless") {
            options.hidden = true;
            options.headless = true;
            continue;
        }
        if (arg == "--auto-control-script") {
            options.auto_control_script = true;
            continue;
        }
        if (arg == "--help") {
            std::printf(
                "usage: icss_tactical_viewer_gui [--host HOST] [--udp-port PORT] [--tcp-port PORT]\n"
                "       [--session-id ID] [--sender-id ID] [--duration-ms MS] [--heartbeat-interval-ms MS]\n"
                "       [--tcp-frame-format json|binary] [--width PX] [--height PX]\n"
                "       [--dump-state PATH] [--font PATH] [--hidden] [--headless]\n"
                "       [--auto-control-script] [--auto-controls Start,Track,...]\n");
            std::exit(0);
        }
        throw std::runtime_error("unknown argument: " + std::string(arg));
    }
    if (options.font_path.empty()) {
        options.font_path = default_font_path();
    }
    return options;
}

icss::core::AssetStatus parse_asset_status(std::string_view value) {
    if (value == "idle") return icss::core::AssetStatus::Idle;
    if (value == "ready") return icss::core::AssetStatus::Ready;
    if (value == "engaging") return icss::core::AssetStatus::Engaging;
    if (value == "complete") return icss::core::AssetStatus::Complete;
    return icss::core::AssetStatus::Idle;
}

icss::core::CommandLifecycle parse_command_status(std::string_view value) {
    if (value == "none") return icss::core::CommandLifecycle::None;
    if (value == "accepted") return icss::core::CommandLifecycle::Accepted;
    if (value == "executing") return icss::core::CommandLifecycle::Executing;
    if (value == "completed") return icss::core::CommandLifecycle::Completed;
    if (value == "rejected") return icss::core::CommandLifecycle::Rejected;
    return icss::core::CommandLifecycle::None;
}

icss::core::JudgmentCode parse_judgment_code(std::string_view value) {
    if (value == "intercept_success") return icss::core::JudgmentCode::InterceptSuccess;
    if (value == "invalid_transition") return icss::core::JudgmentCode::InvalidTransition;
    if (value == "timeout_observed") return icss::core::JudgmentCode::TimeoutObserved;
    return icss::core::JudgmentCode::Pending;
}

icss::core::ConnectionState parse_connection_state(std::string_view value) {
    if (value == "connected") return icss::core::ConnectionState::Connected;
    if (value == "reconnected") return icss::core::ConnectionState::Reconnected;
    if (value == "timed_out") return icss::core::ConnectionState::TimedOut;
    return icss::core::ConnectionState::Disconnected;
}

icss::core::SessionPhase parse_phase(std::string_view value) {
    if (value == "initialized") return icss::core::SessionPhase::Initialized;
    if (value == "detecting") return icss::core::SessionPhase::Detecting;
    if (value == "tracking") return icss::core::SessionPhase::Tracking;
    if (value == "asset_ready") return icss::core::SessionPhase::AssetReady;
    if (value == "command_issued") return icss::core::SessionPhase::CommandIssued;
    if (value == "engaging") return icss::core::SessionPhase::Engaging;
    if (value == "judged") return icss::core::SessionPhase::Judged;
    if (value == "archived") return icss::core::SessionPhase::Archived;
    return icss::core::SessionPhase::Initialized;
}

std::string uppercase_words(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        if (ch == '_') {
            out.push_back(' ');
            continue;
        }
        if (ch >= 'a' && ch <= 'z') {
            out.push_back(static_cast<char>(ch - ('a' - 'A')));
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

std::string phase_banner_label(icss::core::SessionPhase phase) {
    switch (phase) {
    case icss::core::SessionPhase::Initialized: return "INITIALIZED";
    case icss::core::SessionPhase::Detecting: return "DETECTING";
    case icss::core::SessionPhase::Tracking: return "TRACKING";
    case icss::core::SessionPhase::AssetReady: return "INTERCEPTOR READY";
    case icss::core::SessionPhase::CommandIssued: return "COMMAND ACCEPTED";
    case icss::core::SessionPhase::Engaging: return "ENGAGING";
    case icss::core::SessionPhase::Judged: return "JUDGMENT PRODUCED";
    case icss::core::SessionPhase::Archived: return "ARCHIVED";
    }
    return "UNKNOWN";
}

std::string phase_operator_note(icss::core::SessionPhase phase) {
    switch (phase) {
    case icss::core::SessionPhase::Initialized:
        return "Ready for scenario start. No live target track yet.";
    case icss::core::SessionPhase::Detecting:
        return "Target is present. Awaiting track confirmation.";
    case icss::core::SessionPhase::Tracking:
        return "Track is valid. Interceptor can be prepared.";
    case icss::core::SessionPhase::AssetReady:
        return "Interceptor ready. Operator can issue command.";
    case icss::core::SessionPhase::CommandIssued:
        return "Command accepted by authoritative server.";
    case icss::core::SessionPhase::Engaging:
        return "Engagement in progress. Awaiting judgment.";
    case icss::core::SessionPhase::Judged:
        return "Authoritative judgment complete. Review result or archive.";
    case icss::core::SessionPhase::Archived:
        return "Session archived. Reset to begin a new run.";
    }
    return "Unknown phase.";
}

SDL_Color phase_accent(icss::core::SessionPhase phase) {
    switch (phase) {
    case icss::core::SessionPhase::Initialized: return SDL_Color {143, 157, 179, 255};
    case icss::core::SessionPhase::Detecting: return SDL_Color {244, 180, 0, 255};
    case icss::core::SessionPhase::Tracking: return SDL_Color {80, 200, 120, 255};
    case icss::core::SessionPhase::AssetReady: return SDL_Color {77, 171, 247, 255};
    case icss::core::SessionPhase::CommandIssued: return SDL_Color {255, 149, 0, 255};
    case icss::core::SessionPhase::Engaging: return SDL_Color {255, 99, 71, 255};
    case icss::core::SessionPhase::Judged: return SDL_Color {179, 136, 255, 255};
    case icss::core::SessionPhase::Archived: return SDL_Color {130, 130, 130, 255};
    }
    return SDL_Color {143, 157, 179, 255};
}

std::string resilience_summary(const ViewerState& state) {
    const auto freshness = icss::view::freshness_label(state.snapshot);
    if (freshness == "resync") {
        return "Viewer reconnected. Latest snapshot is resynchronizing state.";
    }
    if (freshness == "degraded") {
        return "Packet loss observed. Viewer converged using latest valid snapshot.";
    }
    if (freshness == "stale") {
        return "Viewer state is stale. Treat positional data as low confidence.";
    }
    return "Steady-state propagation. Viewer is following authoritative snapshots.";
}

std::string authoritative_headline(const ViewerState& state) {
    if (state.snapshot.judgment.ready) {
        return "JUDGMENT: " + uppercase_words(icss::core::to_string(state.snapshot.judgment.code));
    }
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return "REJECTED CONTROL: " + uppercase_words(state.control.last_label);
    }
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Executing
        || state.snapshot.command_status == icss::core::CommandLifecycle::Accepted) {
        return "ACTIVE COMMAND: INTERCEPTOR ENGAGING";
    }
    if (!state.recent_server_events.empty()) {
        return "LAST EVENT: " + uppercase_words(state.last_server_event_type);
    }
    return "AWAITING AUTHORITATIVE EVENT";
}

std::string authoritative_badge_label(const ViewerState& state) {
    if (state.snapshot.judgment.ready) {
        return "JUDGED";
    }
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return "REJECTED";
    }
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Executing
        || state.snapshot.command_status == icss::core::CommandLifecycle::Accepted) {
        return "ENGAGING";
    }
    if (!state.recent_server_events.empty()) {
        return "LIVE";
    }
    return "AWAITING";
}

SDL_Color authoritative_badge_color(const ViewerState& state) {
    if (state.snapshot.judgment.ready) {
        return SDL_Color {179, 136, 255, 255};
    }
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return SDL_Color {255, 99, 120, 255};
    }
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Executing
        || state.snapshot.command_status == icss::core::CommandLifecycle::Accepted) {
        return SDL_Color {255, 149, 0, 255};
    }
    if (!state.recent_server_events.empty()) {
        return SDL_Color {120, 190, 255, 255};
    }
    return SDL_Color {143, 157, 179, 255};
}

bool review_available(const ViewerState& state) {
    return state.snapshot.phase == icss::core::SessionPhase::Judged
        || state.snapshot.phase == icss::core::SessionPhase::Archived
        || state.snapshot.judgment.ready;
}

std::string review_panel_text(const ViewerState& state) {
    if (!review_available(state)) {
        return "Review unavailable until authoritative judgment or archive.\n"
               "Use the live log during active control.";
    }
    if (!state.review.loaded) {
        return "Server-side review is available.\n"
               "Press Review to load AAR cursor, judgment, resilience, and event summary.";
    }

    std::string text;
    text += "AAR cursor: " + std::to_string(state.review.cursor_index) + "/" + std::to_string(state.review.total_events) + "\n";
    text += "Judgment: " + state.review.judgment_code + "\n";
    text += "Resilience: " + state.review.resilience_case + "\n";
    text += "Reviewed event: " + state.review.event_type + "\n";
    text += "Summary: " + state.review.event_summary;
    return text;
}

std::string terminal_timeline_text(const ViewerState& state, bool review_mode) {
    std::string out;
    out += review_mode ? "mode=review | source=server_aar\n" : "mode=live | source=server_events\n";
    out += "----------------------------------------\n";
    if (review_mode) {
        const auto body = review_panel_text(state);
        std::size_t start = 0;
        while (start <= body.size()) {
            const auto end = body.find('\n', start);
            const auto line = body.substr(start, end == std::string::npos ? std::string::npos : end - start);
            out += "> " + line + "\n";
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
        return out;
    }

    if (state.recent_server_events.empty()) {
        out += "> waiting for authoritative telemetry or control activity\n";
        return out;
    }
    for (const auto& event : state.recent_server_events) {
        out += "> " + event + "\n";
    }
    return out;
}

std::string control_timeline_message(std::string_view label, bool ok, std::string_view message) {
    return std::string(ok ? "[control accepted] " : "[control rejected] ")
        + std::string(label) + " | " + std::string(message);
}

std::string latest_timeline_entry(const ViewerState& state) {
    if (state.recent_server_events.empty()) {
        return "no timeline activity yet";
    }
    return state.recent_server_events.front();
}

std::string telemetry_event_status(const ViewerState& state) {
    if (state.last_server_event_type == "none") {
        return "pending";
    }
    return "[tick " + std::to_string(state.last_server_event_tick) + "] "
        + state.last_server_event_summary + " (" + state.last_server_event_type + ")";
}

std::string format_fixed_1(float value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

std::string format_fixed_0(std::uint32_t value) {
    return std::to_string(value);
}

icss::core::ScenarioConfig default_viewer_scenario() {
    icss::core::ScenarioConfig scenario;
    scenario.world_width = 576;
    scenario.world_height = 384;
    scenario.target_start_x = 80;
    scenario.target_start_y = 300;
    scenario.target_velocity_x = 5;
    scenario.target_velocity_y = -3;
    scenario.interceptor_start_x = 160;
    scenario.interceptor_start_y = 60;
    scenario.interceptor_speed_per_tick = 14;
    scenario.intercept_radius = 12;
    scenario.engagement_timeout_ticks = 26;
    return scenario;
}

icss::core::ScenarioConfig scenario_profile(std::string_view profile_label) {
    auto scenario = default_viewer_scenario();
    if (profile_label == "Evasive") {
        scenario.target_velocity_x = 7;
        scenario.target_velocity_y = -5;
        scenario.interceptor_speed_per_tick = 12;
        scenario.engagement_timeout_ticks = 20;
    } else if (profile_label == "Timeout") {
        scenario.target_velocity_x = 7;
        scenario.target_velocity_y = -4;
        scenario.interceptor_speed_per_tick = 6;
        scenario.engagement_timeout_ticks = 8;
    }
    return scenario;
}

std::string recommended_control_label(const ViewerState& state) {
    switch (state.snapshot.phase) {
    case icss::core::SessionPhase::Initialized:
        return "Start";
    case icss::core::SessionPhase::Detecting:
        return "Track";
    case icss::core::SessionPhase::Tracking:
        return "Activate";
    case icss::core::SessionPhase::AssetReady:
        return "Command";
    case icss::core::SessionPhase::CommandIssued:
    case icss::core::SessionPhase::Engaging:
    case icss::core::SessionPhase::Judged:
        return "Stop";
    case icss::core::SessionPhase::Archived:
        if (review_available(state) && !state.review.loaded) {
            return "Review";
        }
        return "Reset";
    }
    return "";
}

bool is_profile_button(std::string_view label) {
    return label == "Balanced" || label == "Evasive" || label == "Timeout";
}

void apply_snapshot(ViewerState& state, const icss::protocol::SnapshotPayload& payload) {
    state.snapshot.envelope = payload.envelope;
    state.snapshot.header = payload.header;
    state.snapshot.phase = parse_phase(payload.phase);
    state.snapshot.world_width = payload.world_width;
    state.snapshot.world_height = payload.world_height;
    state.snapshot.target.id = payload.target_id;
    state.snapshot.target.active = payload.target_active;
    state.snapshot.target.position = {payload.target_x, payload.target_y};
    state.snapshot.target_velocity_x = payload.target_velocity_x;
    state.snapshot.target_velocity_y = payload.target_velocity_y;
    state.snapshot.asset.id = payload.asset_id;
    state.snapshot.asset.active = payload.asset_active;
    state.snapshot.asset.position = {payload.asset_x, payload.asset_y};
    state.snapshot.interceptor_speed_per_tick = payload.interceptor_speed_per_tick;
    state.snapshot.intercept_radius = payload.intercept_radius;
    state.snapshot.engagement_timeout_ticks = payload.engagement_timeout_ticks;
    state.snapshot.track.active = payload.tracking_active;
    state.snapshot.track.confidence_pct = payload.track_confidence_pct;
    state.snapshot.asset_status = parse_asset_status(payload.asset_status);
    state.snapshot.command_status = parse_command_status(payload.command_status);
    state.snapshot.judgment.ready = payload.judgment_ready;
    state.snapshot.judgment.code = parse_judgment_code(payload.judgment_code);
    state.command_visual_active = state.snapshot.command_status == icss::core::CommandLifecycle::Accepted
        || state.snapshot.command_status == icss::core::CommandLifecycle::Executing
        || state.snapshot.command_status == icss::core::CommandLifecycle::Completed
        || state.snapshot.judgment.ready;
    state.review.available = review_available(state);
    state.received_snapshot = true;
    ++state.snapshot_count_received;
    if (state.target_history.empty() || state.target_history.back().x != state.snapshot.target.position.x
        || state.target_history.back().y != state.snapshot.target.position.y) {
        state.target_history.push_back(state.snapshot.target.position);
        while (state.target_history.size() > 24) {
            state.target_history.pop_front();
        }
    }
    if (state.asset_history.empty() || state.asset_history.back().x != state.snapshot.asset.position.x
        || state.asset_history.back().y != state.snapshot.asset.position.y) {
        state.asset_history.push_back(state.snapshot.asset.position);
        while (state.asset_history.size() > 24) {
            state.asset_history.pop_front();
        }
    }
}

void apply_telemetry(ViewerState& state, const icss::protocol::TelemetryPayload& payload) {
    state.snapshot.telemetry = payload.sample;
    state.snapshot.viewer_connection = parse_connection_state(payload.connection_state);
    state.received_telemetry = true;
    ++state.telemetry_count_received;
    if (payload.event_type != "none"
        && (payload.event_tick != state.last_server_event_tick
            || payload.event_type != state.last_server_event_type
            || payload.event_summary != state.last_server_event_summary)) {
        state.last_server_event_tick = payload.event_tick;
        state.last_server_event_type = payload.event_type;
        state.last_server_event_summary = payload.event_summary;
        push_timeline_entry(
            state,
            "[tick " + std::to_string(payload.event_tick) + "] " + payload.event_summary + " (" + payload.event_type + ")");
    }
}

#if !defined(_WIN32)
enum class FrameMode {
    Json,
    Binary,
};

FrameMode parse_frame_mode(std::string_view value) {
    if (value == "json") {
        return FrameMode::Json;
    }
    if (value == "binary") {
        return FrameMode::Binary;
    }
    throw std::runtime_error("unsupported tcp frame format: " + std::string(value));
}

class UdpSocket {
public:
    explicit UdpSocket(const std::string& host) {
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) {
            throw std::runtime_error("failed to create udp socket");
        }
        const int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
            throw std::runtime_error("failed to set nonblocking udp socket");
        }

        sockaddr_in bind_addr {};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(0);
        if (::inet_pton(AF_INET, host.c_str(), &bind_addr.sin_addr) != 1) {
            throw std::runtime_error("invalid host for udp bind");
        }
        if (::bind(fd_, reinterpret_cast<const sockaddr*>(&bind_addr), sizeof(bind_addr)) != 0) {
            throw std::runtime_error("failed to bind udp viewer socket");
        }
    }

    ~UdpSocket() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    [[nodiscard]] int get() const { return fd_; }

private:
    int fd_ {-1};
};

class TcpSocket {
public:
    TcpSocket() = default;
    ~TcpSocket() {
        reset();
    }

    void connect_to(const std::string& host, std::uint16_t port) {
        reset();
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            throw std::runtime_error("failed to create tcp socket");
        }
        timeval timeout {};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            throw std::runtime_error("invalid host address");
        }
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            throw std::runtime_error("failed to connect control tcp socket");
        }
    }

    void reset() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] bool connected() const { return fd_ >= 0; }
    [[nodiscard]] int fd() const { return fd_; }

private:
    int fd_ {-1};
};

sockaddr_in make_server_addr(const ViewerOptions& options) {
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(options.udp_port);
    if (::inet_pton(AF_INET, options.host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("invalid host for udp server address");
    }
    return addr;
}

void send_datagram(int fd, const sockaddr_in& target, std::string_view payload) {
    const auto sent = ::sendto(fd,
                               payload.data(),
                               payload.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&target),
                               sizeof(target));
    if (sent < 0) {
        throw std::runtime_error("failed to send udp datagram");
    }
}

void receive_datagrams(int fd, ViewerState& state) {
    char buffer[1024];
    sockaddr_in from {};
    socklen_t len = sizeof(from);
    while (true) {
        const auto received = ::recvfrom(fd,
                                         buffer,
                                         sizeof(buffer),
                                         0,
                                         reinterpret_cast<sockaddr*>(&from),
                                         &len);
        if (received < 0) {
            break;
        }
        const std::string wire(buffer, buffer + received);
        if (wire.rfind("kind=world_snapshot", 0) == 0) {
            apply_snapshot(state, icss::protocol::parse_snapshot(wire));
            continue;
        }
        if (wire.rfind("kind=telemetry", 0) == 0) {
            apply_telemetry(state, icss::protocol::parse_telemetry(wire));
        }
    }
}

void send_frame(int fd, FrameMode mode, std::string_view kind, std::string_view payload) {
    if (mode == FrameMode::Json) {
        const auto line = icss::protocol::encode_json_frame(kind, payload) + "\n";
        if (::send(fd, line.data(), line.size(), 0) < 0) {
            throw std::runtime_error("failed to send json frame");
        }
        return;
    }
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    if (::send(fd, frame.data(), frame.size(), 0) < 0) {
        throw std::runtime_error("failed to send binary frame");
    }
}

icss::protocol::FramedMessage recv_frame(int fd, FrameMode mode) {
    if (mode == FrameMode::Json) {
        std::string line;
        char ch = '\0';
        while (true) {
            const auto received = ::recv(fd, &ch, 1, 0);
            if (received <= 0) {
                throw std::runtime_error("failed to receive json frame");
            }
            if (ch == '\n') {
                return icss::protocol::decode_json_frame(line);
            }
            line.push_back(ch);
        }
    }
    std::string buffer;
    char chunk[512];
    while (true) {
        const auto received = ::recv(fd, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            throw std::runtime_error("failed to receive binary frame");
        }
        buffer.append(chunk, static_cast<std::size_t>(received));
        icss::protocol::FramedMessage frame;
        if (icss::protocol::try_decode_binary_frame(buffer, frame)) {
            return frame;
        }
    }
}

void ensure_control_connected(TcpSocket& socket,
                              ViewerState& state,
                              const ViewerOptions& options,
                              FrameMode mode) {
    if (state.control.connected && socket.connected()) {
        return;
    }
    socket.connect_to(options.host, options.tcp_port);
    send_frame(socket.fd(),
               mode,
               "session_join",
               icss::protocol::serialize(icss::protocol::SessionJoinPayload{{options.session_id, options.sender_id, state.control.sequence++},
                                                                            "command_console"}));
    const auto frame = recv_frame(socket.fd(), mode);
    if (frame.kind != "command_ack") {
        throw std::runtime_error("expected command_ack for session_join");
    }
    const auto ack = icss::protocol::parse_command_ack(frame.payload);
    state.control.connected = ack.accepted;
    state.control.last_ok = ack.accepted;
    state.control.last_label = "join";
    state.control.last_message = ack.reason;
}
#endif

SDL_Color rgba(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    return SDL_Color {r, g, b, a};
}

void fill_panel(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color fill, SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void draw_text(SDL_Renderer* renderer,
               TTF_Font* font,
               int x,
               int y,
               SDL_Color color,
               const std::string& text,
               int wrap_width = 0) {
    SDL_Surface* surface = wrap_width > 0
        ? TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color, wrap_width)
        : TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        throw std::runtime_error("failed to render text surface");
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        throw std::runtime_error("failed to create text texture");
    }
    SDL_Rect dest {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

float map_axis(int value, int world_limit, int screen_origin, int screen_extent) {
    if (world_limit <= 1) {
        return static_cast<float>(screen_origin);
    }
    const int clamped = std::clamp(value, 0, world_limit - 1);
    return static_cast<float>(screen_origin) + (static_cast<float>(clamped) * static_cast<float>(screen_extent - 1) / static_cast<float>(world_limit - 1));
}

SDL_FPoint map_point_for_entity(const SDL_Rect& map_rect,
                                int world_width,
                                int world_height,
                                const icss::core::Vec2& position) {
    return SDL_FPoint {
        map_axis(position.x, world_width, map_rect.x, map_rect.w),
        map_axis(position.y, world_height, map_rect.y, map_rect.h),
    };
}

bool rects_overlap(const SDL_Rect& lhs, const SDL_Rect& rhs) {
    return lhs.x < rhs.x + rhs.w
        && lhs.x + lhs.w > rhs.x
        && lhs.y < rhs.y + rhs.h
        && lhs.y + lhs.h > rhs.y;
}

std::vector<Button> make_control_buttons(const SDL_Rect& control_panel) {
    const int row1_y = control_panel.y + 84;
    const int row2_y = control_panel.y + 126;
    const int row3_y = control_panel.y + 176;
    return {
        {"Start", {control_panel.x + 12, row1_y, 82, 32}},
        {"Track", {control_panel.x + 102, row1_y, 82, 32}},
        {"Activate", {control_panel.x + 192, row1_y, 96, 32}},
        {"Command", {control_panel.x + 12, row2_y, 96, 32}},
        {"Stop", {control_panel.x + 116, row2_y, 72, 32}},
        {"Reset", {control_panel.x + 196, row2_y, 82, 32}},
        {"Review", {control_panel.x + 286, row2_y, 72, 32}},
        {"Balanced", {control_panel.x + 12, row3_y, 102, 24}},
        {"Evasive", {control_panel.x + 122, row3_y, 102, 24}},
        {"Timeout", {control_panel.x + 232, row3_y, 102, 24}},
    };
}

GuiLayout build_layout(const ViewerOptions& options) {
    constexpr int margin = 24;
    constexpr int header_x = 24;
    constexpr int header_y = 18;
    constexpr int header_h = 64;
    constexpr int gap_after_header = 36;
    constexpr int gap_after_map = 18;
    constexpr int gap_before_event_panel = 18;
    constexpr int grid_width = 24;
    constexpr int grid_height = 16;
    constexpr int cell_px = 24;
    constexpr int entity_panel_h = 98;
    constexpr int right_panel_gap = 12;
    constexpr int left_panel_w = 330;

    GuiLayout layout;
    layout.header_panel = {header_x, header_y, options.width - (margin * 2), header_h};
    layout.phase_strip = {layout.header_panel.x + 12, layout.header_panel.y + 12, 330, layout.header_panel.h - 24};
    const int content_y = header_y + header_h + gap_after_header;
    layout.map_rect = {margin, content_y, grid_width * cell_px, grid_height * cell_px};
    layout.entity_panel = {margin, layout.map_rect.y + layout.map_rect.h + gap_after_map, layout.map_rect.w, entity_panel_h};
    layout.panel_x = layout.map_rect.x + layout.map_rect.w + margin;
    const int stacked_column_total_h = layout.map_rect.h + layout.entity_panel.h;
    const int panel_h = stacked_column_total_h / 2;
    layout.phase_panel = {layout.panel_x, content_y, left_panel_w, panel_h};
    const int right_panel_x = layout.panel_x + left_panel_w + right_panel_gap;
    layout.decision_panel = {right_panel_x, content_y, options.width - right_panel_x - margin, panel_h};
    layout.resilience_panel = {layout.panel_x, content_y + panel_h + right_panel_gap, 330, panel_h};
    layout.control_panel = {right_panel_x, content_y + panel_h + right_panel_gap, options.width - right_panel_x - margin, panel_h};
    const int upper_panel_bottom = std::max(layout.entity_panel.y + layout.entity_panel.h,
                                            std::max(layout.resilience_panel.y + layout.resilience_panel.h,
                                                     layout.control_panel.y + layout.control_panel.h));
    layout.event_panel = {margin, upper_panel_bottom + gap_before_event_panel, options.width - (margin * 2),
                          options.height - (upper_panel_bottom + gap_before_event_panel) - margin};
    layout.buttons = make_control_buttons(layout.control_panel);

    const std::array<SDL_Rect, 7> panels {
        layout.header_panel,
        layout.map_rect,
        layout.entity_panel,
        layout.phase_panel,
        layout.decision_panel,
        layout.resilience_panel,
        layout.control_panel,
    };
    for (std::size_t i = 0; i < panels.size(); ++i) {
        for (std::size_t j = i + 1; j < panels.size(); ++j) {
            if (rects_overlap(panels[i], panels[j])) {
                throw std::runtime_error("gui layout overlap detected");
            }
        }
    }
    if (rects_overlap(layout.entity_panel, layout.event_panel)
        || rects_overlap(layout.phase_panel, layout.event_panel)
        || rects_overlap(layout.decision_panel, layout.event_panel)
        || rects_overlap(layout.resilience_panel, layout.event_panel)
        || rects_overlap(layout.control_panel, layout.event_panel)) {
        throw std::runtime_error("gui event panel overlaps another panel");
    }
    for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
        const auto& rect = layout.buttons[i].rect;
        const auto inside_control = rect.x >= layout.control_panel.x
            && rect.y >= layout.control_panel.y
            && rect.x + rect.w <= layout.control_panel.x + layout.control_panel.w
            && rect.y + rect.h <= layout.control_panel.y + layout.control_panel.h;
        if (!inside_control) {
            throw std::runtime_error("gui control button exceeds control panel bounds");
        }
        for (std::size_t j = i + 1; j < layout.buttons.size(); ++j) {
            if (rects_overlap(rect, layout.buttons[j].rect)) {
                throw std::runtime_error("gui control buttons overlap");
            }
        }
    }
    return layout;
}

void render_gui(SDL_Renderer* renderer,
                TTF_Font* title_font,
                TTF_Font* body_font,
                const ViewerState& state,
                const ViewerOptions& options) {
    constexpr int reference_step = 48;
    const auto layout = build_layout(options);
    const auto& header_panel = layout.header_panel;
    const auto& phase_strip = layout.phase_strip;
    const auto& map_rect = layout.map_rect;
    const auto& entity_panel = layout.entity_panel;
    const auto& phase_panel = layout.phase_panel;
    const auto& decision_panel = layout.decision_panel;
    const auto& resilience_panel = layout.resilience_panel;
    const auto& control_panel = layout.control_panel;
    const auto& event_panel = layout.event_panel;
    const auto& buttons = layout.buttons;
    const auto phase_title = phase_banner_label(state.snapshot.phase);
    const auto phase_note = phase_operator_note(state.snapshot.phase);
    const auto phase_color = phase_accent(state.snapshot.phase);
    const auto authoritative_title = authoritative_headline(state);
    const auto authoritative_badge = authoritative_badge_label(state);
    const auto authoritative_color = authoritative_badge_color(state);
    const auto resilience_note = resilience_summary(state);
    const auto recommended_control = recommended_control_label(state);
    const int title_font_h = TTF_FontHeight(title_font);
    const int body_font_h = TTF_FontHeight(body_font);
    const int header_text_gap = 4;
    const int header_text_total_h = title_font_h + header_text_gap + body_font_h;
    const int header_text_y = header_panel.y + std::max(0, (header_panel.h - header_text_total_h) / 2);

    SDL_SetRenderDrawColor(renderer, 15, 18, 24, 255);
    SDL_RenderClear(renderer);

    fill_panel(renderer, header_panel, rgba(18, 24, 32), rgba(56, 66, 86));
    SDL_SetRenderDrawColor(renderer, phase_color.r, phase_color.g, phase_color.b, 255);
    SDL_RenderFillRect(renderer, &phase_strip);
    draw_text(renderer, title_font, phase_strip.x + 14, phase_strip.y + 8, rgba(10, 12, 16), phase_title);
    const SDL_Rect status_badge {header_panel.x + header_panel.w - 196, header_panel.y + (header_panel.h - 28) / 2, 184, 28};
    fill_panel(renderer, status_badge, rgba(28, 34, 46), authoritative_color);
    draw_text(renderer,
              body_font,
              status_badge.x + 10,
              status_badge.y + std::max(0, (status_badge.h - body_font_h) / 2),
              authoritative_color,
              authoritative_badge,
              status_badge.w - 20);
    draw_text(renderer, title_font, header_panel.x + 360, header_text_y, rgba(236, 239, 244), "ICSS Tactical Viewer");
    draw_text(renderer,
              body_font,
              header_panel.x + 360,
              header_text_y + title_font_h + header_text_gap,
              rgba(200, 208, 220),
              phase_note,
              status_badge.x - (header_panel.x + 372));

    fill_panel(renderer, map_rect, rgba(36, 41, 52), rgba(74, 80, 96));
    SDL_SetRenderDrawColor(renderer, 74, 80, 96, 255);
    for (int x = 0; x <= map_rect.w; x += reference_step) {
        const int px = map_rect.x + x;
        SDL_RenderDrawLine(renderer, px, map_rect.y, px, map_rect.y + map_rect.h);
    }
    for (int y = 0; y <= map_rect.h; y += reference_step) {
        const int py = map_rect.y + y;
        SDL_RenderDrawLine(renderer, map_rect.x, py, map_rect.x + map_rect.w, py);
    }
    for (int x = 0; x <= state.snapshot.world_width; x += 96) {
        const int px = static_cast<int>(map_axis(x, state.snapshot.world_width, map_rect.x, map_rect.w));
        draw_text(renderer, body_font, px + 2, map_rect.y - 22, rgba(140, 149, 168), std::to_string(x));
    }
    for (int y = 0; y <= state.snapshot.world_height; y += 96) {
        const int py = static_cast<int>(map_axis(y, state.snapshot.world_height, map_rect.y, map_rect.h));
        draw_text(renderer, body_font, map_rect.x - 24, py + 2, rgba(140, 149, 168), std::to_string(y));
    }

    auto draw_history = [&](const std::deque<icss::core::Vec2>& history, SDL_Color color) {
        if (history.size() < 2) {
            return;
        }
        std::vector<SDL_Point> points;
        points.reserve(history.size());
        for (const auto& pos : history) {
            points.push_back(SDL_Point {
                static_cast<int>(map_axis(pos.x, state.snapshot.world_width, map_rect.x, map_rect.w)),
                static_cast<int>(map_axis(pos.y, state.snapshot.world_height, map_rect.y, map_rect.h)),
            });
        }
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 150);
        SDL_RenderDrawLines(renderer, points.data(), static_cast<int>(points.size()));
    };
    draw_history(state.target_history, rgba(244, 67, 54, 160));
    draw_history(state.asset_history, rgba(66, 165, 245, 160));
    if (state.snapshot.track.active && state.snapshot.target.active && state.snapshot.asset.active) {
        const auto target_center = map_point_for_entity(map_rect, state.snapshot.world_width, state.snapshot.world_height, state.snapshot.target.position);
        const auto asset_center = map_point_for_entity(map_rect, state.snapshot.world_width, state.snapshot.world_height, state.snapshot.asset.position);
        SDL_SetRenderDrawColor(renderer, 255, 214, 102, 180);
        SDL_RenderDrawLine(renderer,
                           static_cast<int>(asset_center.x),
                           static_cast<int>(asset_center.y),
                           static_cast<int>(target_center.x),
                           static_cast<int>(target_center.y));
        if (state.command_visual_active || state.snapshot.command_status != icss::core::CommandLifecycle::None) {
            SDL_SetRenderDrawColor(renderer, 255, 149, 0, 255);
            SDL_RenderDrawLine(renderer,
                               static_cast<int>(asset_center.x) - 1,
                               static_cast<int>(asset_center.y),
                               static_cast<int>(target_center.x) - 1,
                               static_cast<int>(target_center.y));
            SDL_RenderDrawLine(renderer,
                               static_cast<int>(asset_center.x) + 1,
                               static_cast<int>(asset_center.y),
                               static_cast<int>(target_center.x) + 1,
                               static_cast<int>(target_center.y));
            SDL_Rect target_box {
                static_cast<int>(target_center.x) - 10,
                static_cast<int>(target_center.y) - 10,
                20,
                20,
            };
            SDL_RenderDrawRect(renderer, &target_box);
        }
    }

    auto draw_entity = [&](const icss::core::EntityState& entity, SDL_Color color) {
        SDL_Rect rect {
            static_cast<int>(map_axis(entity.position.x, state.snapshot.world_width, map_rect.x, map_rect.w)) - 4,
            static_cast<int>(map_axis(entity.position.y, state.snapshot.world_height, map_rect.y, map_rect.h)) - 4,
            8,
            8,
        };
        if (entity.active) {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 240, 244, 255, 255);
            SDL_RenderDrawRect(renderer, &rect);
        } else {
            SDL_SetRenderDrawColor(renderer, color.r / 3, color.g / 3, color.b / 3, 255);
            SDL_RenderDrawRect(renderer, &rect);
        }
    };
    draw_entity(state.snapshot.target, rgba(244, 67, 54));
    draw_entity(state.snapshot.asset, rgba(66, 165, 245));

    fill_panel(renderer, phase_panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, title_font, phase_panel.x + 12, phase_panel.y + 10, rgba(141, 211, 199), "Mission Phase");
    const std::array<std::pair<icss::core::SessionPhase, const char*>, 8> phase_flow {{
        {icss::core::SessionPhase::Initialized, "INITIALIZED"},
        {icss::core::SessionPhase::Detecting, "DETECTING"},
        {icss::core::SessionPhase::Tracking, "TRACKING"},
        {icss::core::SessionPhase::AssetReady, "INTERCEPTOR READY"},
        {icss::core::SessionPhase::CommandIssued, "COMMAND ACCEPTED"},
        {icss::core::SessionPhase::Engaging, "ENGAGING"},
        {icss::core::SessionPhase::Judged, "JUDGMENT PRODUCED"},
        {icss::core::SessionPhase::Archived, "ARCHIVED"},
    }};
    for (std::size_t index = 0; index < phase_flow.size(); ++index) {
        const SDL_Rect row {phase_panel.x + 12, phase_panel.y + 42 + static_cast<int>(index) * 20, phase_panel.w - 24, 18};
        const auto row_phase = phase_flow[index].first;
        const auto active = row_phase == state.snapshot.phase;
        const auto completed = static_cast<int>(row_phase) < static_cast<int>(state.snapshot.phase);
        if (active) {
            SDL_SetRenderDrawColor(renderer, phase_color.r, phase_color.g, phase_color.b, 255);
            SDL_RenderFillRect(renderer, &row);
            draw_text(renderer, body_font, row.x + 8, row.y + 1, rgba(10, 12, 16), phase_flow[index].second);
        } else {
            SDL_SetRenderDrawColor(renderer, completed ? 68 : 42, completed ? 91 : 50, completed ? 72 : 58, 255);
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 66, 74, 92, 255);
            SDL_RenderDrawRect(renderer, &row);
            draw_text(renderer,
                      body_font,
                      row.x + 8,
                      row.y + 1,
                      completed ? rgba(176, 216, 184) : rgba(148, 156, 172),
                      phase_flow[index].second);
        }
    }

    fill_panel(renderer, decision_panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, title_font, decision_panel.x + 12, decision_panel.y + 10, rgba(255, 179, 102), "Authoritative Status");
    std::string decision_block;
    decision_block += "Last telemetry event: " + telemetry_event_status(state) + "\n";
    decision_block += "Latest timeline entry: " + latest_timeline_entry(state) + "\n";
    decision_block += std::string(state.control.last_ok ? "Last accepted control: " : "Last rejected control: ")
        + state.control.last_label + " | " + state.control.last_message + "\n";
    decision_block += "Judgment: " + std::string(icss::core::to_string(state.snapshot.judgment.code))
        + " | Command lifecycle: " + std::string(icss::core::to_string(state.snapshot.command_status)) + "\n";
    decision_block += "Interceptor status: " + std::string(icss::core::to_string(state.snapshot.asset_status))
        + " | AAR cursor: live";
    draw_text(renderer,
              body_font,
              decision_panel.x + 12,
              decision_panel.y + 44,
              rgba(220, 223, 230),
              decision_block,
              decision_panel.w - 24);
    fill_panel(renderer, resilience_panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, title_font, resilience_panel.x + 12, resilience_panel.y + 10, rgba(129, 199, 255), "Resilience / Telemetry");
    std::string resilience_block;
    resilience_block += "Connection: " + std::string(icss::core::to_string(state.snapshot.viewer_connection))
        + " | Freshness: " + icss::view::freshness_label(state.snapshot) + "\n";
    resilience_block += "Tick: " + std::to_string(state.snapshot.telemetry.tick)
        + " | Snapshot: " + std::to_string(state.snapshot.header.snapshot_sequence) + "\n";
    resilience_block += "Latency: " + format_fixed_0(state.snapshot.telemetry.latency_ms)
        + " ms | Loss: " + format_fixed_1(state.snapshot.telemetry.packet_loss_pct) + " %";
    if (state.snapshot.telemetry.packet_loss_pct > 0.0F) {
        resilience_block += " | degraded sample";
    } else {
        resilience_block += " | stable";
    }
    resilience_block += "\n";
    resilience_block += "I/O: snapshots=" + std::to_string(state.snapshot_count_received)
        + ", telemetry=" + std::to_string(state.telemetry_count_received)
        + ", heartbeats=" + std::to_string(state.heartbeat_count) + "\n";
    resilience_block += "Note: " + resilience_note;
    draw_text(renderer,
              body_font,
              resilience_panel.x + 12,
              resilience_panel.y + 44,
              rgba(220, 223, 230),
              resilience_block,
              resilience_panel.w - 24);

    fill_panel(renderer, control_panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, title_font, control_panel.x + 12, control_panel.y + 10, rgba(141, 211, 199), "Control Panel");
    const SDL_Rect next_hint_box {control_panel.x + 12, control_panel.y + 40, control_panel.w - 24, 30};
    fill_panel(renderer, next_hint_box, rgba(22, 27, 36), rgba(56, 66, 86));
    const SDL_Rect profile_box {control_panel.x + 12, control_panel.y + 204, control_panel.w - 24, 28};
    fill_panel(renderer, profile_box, rgba(22, 27, 36), rgba(56, 66, 86));
    for (const auto& button : buttons) {
        const bool is_review_button = button.label == "Review";
        const bool is_reset_button = button.label == "Reset";
        const bool is_profile = is_profile_button(button.label);
        const bool enabled = !is_review_button || review_available(state);
        const bool recommended = enabled && button.label == recommended_control;
        const auto fill = is_reset_button
            ? (enabled ? rgba(92, 42, 52) : rgba(56, 36, 40))
            : (is_profile
                ? (state.profile_label == button.label ? rgba(76, 92, 126) : rgba(34, 44, 58))
                : (recommended ? phase_color : (enabled ? rgba(28, 55, 86) : rgba(38, 38, 38))));
        const auto border = is_reset_button
            ? (enabled ? rgba(224, 108, 146) : rgba(96, 72, 80))
            : (is_profile
                ? (state.profile_label == button.label ? rgba(174, 195, 255) : rgba(88, 102, 126))
                : (recommended ? rgba(240, 244, 255) : (enabled ? rgba(91, 126, 168) : rgba(72, 72, 72))));
        const auto text = is_reset_button
            ? (enabled ? rgba(255, 220, 230) : rgba(148, 156, 172))
            : (is_profile
                ? (state.profile_label == button.label ? rgba(238, 245, 255) : rgba(188, 198, 214))
                : (recommended ? rgba(10, 12, 16) : (enabled ? rgba(238, 245, 255) : rgba(148, 156, 172))));
        if (is_reset_button && enabled) {
            const SDL_Rect glow_outer {button.rect.x - 1, button.rect.y - 1, button.rect.w + 2, button.rect.h + 2};
            SDL_SetRenderDrawColor(renderer, 255, 128, 168, 96);
            SDL_RenderDrawRect(renderer, &glow_outer);
        }
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &button.rect);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &button.rect);
        if (is_reset_button) {
            const SDL_Rect inner {button.rect.x + 1, button.rect.y + 1, button.rect.w - 2, button.rect.h - 2};
            SDL_SetRenderDrawColor(renderer, 255, 168, 196, enabled ? 168 : 84);
            SDL_RenderDrawRect(renderer, &inner);
        }
        if (recommended) {
            const SDL_Rect glow {button.rect.x - 2, button.rect.y - 2, button.rect.w + 4, button.rect.h + 4};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
            SDL_RenderDrawRect(renderer, &glow);
        }
        draw_text(renderer, body_font, button.rect.x + 10, button.rect.y + 7, text, button.label);
    }
    draw_text(renderer,
              body_font,
              next_hint_box.x + 10,
              next_hint_box.y + 7,
              rgba(148, 156, 172),
              recommended_control.empty()
                  ? "Next recommended control: none"
                  : "Next recommended control: " + recommended_control,
              next_hint_box.w - 20);
    draw_text(renderer,
              body_font,
              profile_box.x + 10,
              profile_box.y + 7,
              rgba(148, 156, 172),
              "Profile=" + state.profile_label
                  + " | vT=(" + std::to_string(state.planned_scenario.target_velocity_x) + "," + std::to_string(state.planned_scenario.target_velocity_y) + ")"
                  + " | vI=" + std::to_string(state.planned_scenario.interceptor_speed_per_tick)
                  + " | timeout=" + std::to_string(state.planned_scenario.engagement_timeout_ticks),
              profile_box.w - 20);
    fill_panel(renderer, entity_panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, title_font, entity_panel.x + 12, entity_panel.y + 10, rgba(220, 223, 230), "Entity Picture");
    draw_text(renderer, body_font, entity_panel.x + 170, entity_panel.y + 14, rgba(140, 149, 168), "target=red | interceptor=blue | command=orange");
    std::string entity_block;
    entity_block += "Target " + state.snapshot.target.id + " @ (" + std::to_string(state.snapshot.target.position.x)
        + ", " + std::to_string(state.snapshot.target.position.y) + ") active=" + (state.snapshot.target.active ? "yes" : "no") + "\n";
    entity_block += "Interceptor " + state.snapshot.asset.id + " @ (" + std::to_string(state.snapshot.asset.position.x)
        + ", " + std::to_string(state.snapshot.asset.position.y) + ") active=" + (state.snapshot.asset.active ? "yes" : "no") + "\n";
    entity_block += "Tracking " + std::string(state.snapshot.track.active ? "ON" : "OFF")
        + " (" + std::to_string(state.snapshot.track.confidence_pct) + "%)"
        + " | Command visual " + (state.command_visual_active ? "ON" : "OFF");
    draw_text(renderer, body_font, entity_panel.x + 12, entity_panel.y + 42, rgba(220, 223, 230), entity_block, entity_panel.w - 24);

    fill_panel(renderer, event_panel, rgba(8, 10, 14), rgba(54, 60, 78));
    const bool showing_review = state.review.visible && state.review.loaded;
    draw_text(renderer,
              title_font,
              event_panel.x + 12,
              event_panel.y + 10,
              rgba(164, 215, 150),
              "Event Timeline");
    draw_text(renderer,
              body_font,
              event_panel.x + 190,
              event_panel.y + 14,
              rgba(120, 128, 144),
              showing_review ? "[review mode]" : "[live mode]");

    const std::string events_block = terminal_timeline_text(state, showing_review);
    draw_text(renderer,
              body_font,
              event_panel.x + 12,
              event_panel.y + 46,
              rgba(187, 255, 187),
              events_block,
              event_panel.w - 24);

    SDL_RenderPresent(renderer);
}

void write_dump_state(const std::filesystem::path& path, const ViewerState& state) {
    if (path.empty()) {
        return;
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "{\n";
    out << "  \"schema_version\": \"icss-gui-viewer-state-v1\",\n";
    out << "  \"received_snapshot\": " << (state.received_snapshot ? "true" : "false") << ",\n";
    out << "  \"received_telemetry\": " << (state.received_telemetry ? "true" : "false") << ",\n";
    out << "  \"snapshot_sequence\": " << state.snapshot.header.snapshot_sequence << ",\n";
    out << "  \"tick\": " << state.snapshot.telemetry.tick << ",\n";
    out << "  \"phase\": \"" << icss::core::to_string(state.snapshot.phase) << "\",\n";
    out << "  \"phase_banner\": \"" << escape_json(phase_banner_label(state.snapshot.phase)) << "\",\n";
    out << "  \"profile_label\": \"" << escape_json(state.profile_label) << "\",\n";
    out << "  \"connection_state\": \"" << icss::core::to_string(state.snapshot.viewer_connection) << "\",\n";
    out << "  \"freshness\": \"" << icss::view::freshness_label(state.snapshot) << "\",\n";
    out << "  \"resilience_summary\": \"" << escape_json(resilience_summary(state)) << "\",\n";
    out << "  \"judgment_code\": \"" << icss::core::to_string(state.snapshot.judgment.code) << "\",\n";
    out << "  \"heartbeat_count\": " << state.heartbeat_count << ",\n";
    out << "  \"last_server_event_tick\": " << state.last_server_event_tick << ",\n";
    out << "  \"last_server_event_type\": \"" << escape_json(state.last_server_event_type) << "\",\n";
    out << "  \"last_server_event_summary\": \"" << escape_json(state.last_server_event_summary) << "\",\n";
    out << "  \"authoritative_headline\": \"" << escape_json(authoritative_headline(state)) << "\",\n";
    out << "  \"recommended_control\": \"" << escape_json(recommended_control_label(state)) << "\",\n";
    out << "  \"review_available\": " << (review_available(state) ? "true" : "false") << ",\n";
    out << "  \"review_loaded\": " << (state.review.loaded ? "true" : "false") << ",\n";
    out << "  \"review_visible\": " << (state.review.visible ? "true" : "false") << ",\n";
    out << "  \"review_cursor_index\": " << state.review.cursor_index << ",\n";
    out << "  \"review_total_events\": " << state.review.total_events << ",\n";
    out << "  \"review_judgment_code\": \"" << escape_json(state.review.judgment_code) << "\",\n";
    out << "  \"review_resilience_case\": \"" << escape_json(state.review.resilience_case) << "\",\n";
    out << "  \"review_event_type\": \"" << escape_json(state.review.event_type) << "\",\n";
    out << "  \"review_event_summary\": \"" << escape_json(state.review.event_summary) << "\",\n";
    out << "  \"last_control_label\": \"" << escape_json(state.control.last_label) << "\",\n";
    out << "  \"last_control_message\": \"" << escape_json(state.control.last_message) << "\",\n";
    out << "  \"asset_status\": \"" << icss::core::to_string(state.snapshot.asset_status) << "\",\n";
    out << "  \"command_status\": \"" << icss::core::to_string(state.snapshot.command_status) << "\",\n";
    out << "  \"command_visual_active\": " << (state.command_visual_active ? "true" : "false") << ",\n";
    out << "  \"target_active\": " << (state.snapshot.target.active ? "true" : "false") << ",\n";
    out << "  \"asset_active\": " << (state.snapshot.asset.active ? "true" : "false") << ",\n";
    out << "  \"target_x\": " << state.snapshot.target.position.x << ",\n";
    out << "  \"target_y\": " << state.snapshot.target.position.y << ",\n";
    out << "  \"target_velocity_x\": " << state.snapshot.target_velocity_x << ",\n";
    out << "  \"target_velocity_y\": " << state.snapshot.target_velocity_y << ",\n";
    out << "  \"asset_x\": " << state.snapshot.asset.position.x << ",\n";
    out << "  \"asset_y\": " << state.snapshot.asset.position.y << ",\n";
    out << "  \"world_width\": " << state.snapshot.world_width << ",\n";
    out << "  \"world_height\": " << state.snapshot.world_height << ",\n";
    out << "  \"interceptor_speed_per_tick\": " << state.snapshot.interceptor_speed_per_tick << ",\n";
    out << "  \"engagement_timeout_ticks\": " << state.snapshot.engagement_timeout_ticks << ",\n";
    out << "  \"recent_event_count\": " << state.recent_server_events.size() << "\n";
    out << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_args(argc, argv);
        if (options.headless) {
            SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        }
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
        if (TTF_Init() != 0) {
            throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
        }

        Uint32 window_flags = options.hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
        SDL_Window* window = SDL_CreateWindow("ICSS Tactical Viewer",
                                              SDL_WINDOWPOS_CENTERED,
                                              SDL_WINDOWPOS_CENTERED,
                                              options.width,
                                              options.height,
                                              window_flags);
        if (!window) {
            throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        }
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        }

        TTF_Font* title_font = TTF_OpenFont(options.font_path.string().c_str(), 20);
        TTF_Font* body_font = TTF_OpenFont(options.font_path.string().c_str(), 15);
        if (!title_font || !body_font) {
            throw std::runtime_error(std::string("failed to open font: ") + TTF_GetError());
        }

        ViewerState state;
        state.planned_scenario = default_viewer_scenario();
        state.snapshot.target.id = "target-alpha";
        state.snapshot.asset.id = "asset-interceptor";
        state.snapshot.world_width = state.planned_scenario.world_width;
        state.snapshot.world_height = state.planned_scenario.world_height;
        state.snapshot.target.position = {state.planned_scenario.target_start_x, state.planned_scenario.target_start_y};
        state.snapshot.asset.position = {state.planned_scenario.interceptor_start_x, state.planned_scenario.interceptor_start_y};
        state.snapshot.target_velocity_x = state.planned_scenario.target_velocity_x;
        state.snapshot.target_velocity_y = state.planned_scenario.target_velocity_y;
        state.snapshot.interceptor_speed_per_tick = state.planned_scenario.interceptor_speed_per_tick;
        state.snapshot.intercept_radius = state.planned_scenario.intercept_radius;
        state.snapshot.engagement_timeout_ticks = state.planned_scenario.engagement_timeout_ticks;
        state.snapshot.viewer_connection = icss::core::ConnectionState::Disconnected;

#if !defined(_WIN32)
        UdpSocket socket(options.host);
        TcpSocket control_socket;
        const auto frame_mode = parse_frame_mode(options.tcp_frame_format);
        const auto server_addr = make_server_addr(options);
        std::uint64_t sequence = 1;
        const auto send_join = [&]() {
            const icss::protocol::SessionJoinPayload join {
                {options.session_id, options.sender_id, sequence++},
                "tactical_viewer",
            };
            send_datagram(socket.get(), server_addr, icss::protocol::serialize(join));
        };
        send_join();
#endif

        bool running = true;
        const auto start_ticks = SDL_GetTicks64();
        auto next_heartbeat = start_ticks + options.heartbeat_interval_ms;
        const auto perform_control_action = [&](std::string_view action_label) {
            if (is_profile_button(action_label)) {
                state.profile_label = std::string(action_label);
                state.planned_scenario = scenario_profile(action_label);
                state.snapshot.world_width = state.planned_scenario.world_width;
                state.snapshot.world_height = state.planned_scenario.world_height;
                state.snapshot.target.position = {state.planned_scenario.target_start_x, state.planned_scenario.target_start_y};
                state.snapshot.asset.position = {state.planned_scenario.interceptor_start_x, state.planned_scenario.interceptor_start_y};
                state.snapshot.target_velocity_x = state.planned_scenario.target_velocity_x;
                state.snapshot.target_velocity_y = state.planned_scenario.target_velocity_y;
                state.snapshot.interceptor_speed_per_tick = state.planned_scenario.interceptor_speed_per_tick;
                state.snapshot.intercept_radius = state.planned_scenario.intercept_radius;
                state.snapshot.engagement_timeout_ticks = state.planned_scenario.engagement_timeout_ticks;
                state.control.last_ok = true;
                state.control.last_label = std::string(action_label);
                state.control.last_message = "scenario profile selected";
                push_timeline_entry(state, "[profile] " + std::string(action_label) + " selected");
                return;
            }
            ensure_control_connected(control_socket, state, options, frame_mode);
            const auto send_and_expect_ack = [&](std::string_view kind, std::string payload) {
                send_frame(control_socket.fd(), frame_mode, kind, payload);
                const auto frame = recv_frame(control_socket.fd(), frame_mode);
                if (frame.kind != "command_ack") {
                    throw std::runtime_error("expected command_ack after " + std::string(kind));
                }
                const auto ack = icss::protocol::parse_command_ack(frame.payload);
                state.control.last_ok = ack.accepted;
                state.control.last_label = std::string(action_label);
                state.control.last_message = ack.reason;
                push_timeline_entry(state, control_timeline_message(action_label, ack.accepted, ack.reason));
                if (ack.accepted) {
                    if (action_label != "Review") {
                        state.review.visible = false;
                    }
                    if (action_label == "Start") {
                        state.snapshot.phase = icss::core::SessionPhase::Detecting;
                        state.snapshot.target.active = true;
                        state.snapshot.command_status = icss::core::CommandLifecycle::None;
                        state.snapshot.asset_status = icss::core::AssetStatus::Idle;
                        state.command_visual_active = false;
                    } else if (action_label == "Activate") {
                        state.snapshot.phase = icss::core::SessionPhase::AssetReady;
                        state.snapshot.asset.active = true;
                        state.snapshot.asset_status = icss::core::AssetStatus::Ready;
                    } else if (action_label == "Track") {
                        state.snapshot.phase = icss::core::SessionPhase::Tracking;
                        state.snapshot.track.active = true;
                        state.snapshot.track.confidence_pct = std::max(state.snapshot.track.confidence_pct, 82);
                    } else if (action_label == "Command") {
                        state.snapshot.phase = icss::core::SessionPhase::CommandIssued;
                        state.snapshot.command_status = icss::core::CommandLifecycle::Accepted;
                        state.snapshot.asset_status = icss::core::AssetStatus::Engaging;
                        state.command_visual_active = true;
                    } else if (action_label == "Stop") {
                        state.snapshot.phase = icss::core::SessionPhase::Archived;
                    } else if (action_label == "Reset") {
                        state.snapshot.phase = icss::core::SessionPhase::Initialized;
                        state.snapshot.target.active = false;
                        state.snapshot.asset.active = false;
                        state.snapshot.track.active = false;
                        state.snapshot.track.confidence_pct = 0;
                        state.snapshot.command_status = icss::core::CommandLifecycle::None;
                        state.snapshot.asset_status = icss::core::AssetStatus::Idle;
                        state.snapshot.judgment = {};
                        state.command_visual_active = false;
                        state.review = {};
                    }
                }
            };
            if (action_label == "Start") {
                send_and_expect_ack(
                    "scenario_start",
                    icss::protocol::serialize(icss::protocol::ScenarioStartPayload{
                        {options.session_id, options.sender_id, state.control.sequence++},
                        "basic_intercept_training",
                        state.planned_scenario.world_width,
                        state.planned_scenario.world_height,
                        state.planned_scenario.target_start_x,
                        state.planned_scenario.target_start_y,
                        state.planned_scenario.target_velocity_x,
                        state.planned_scenario.target_velocity_y,
                        state.planned_scenario.interceptor_start_x,
                        state.planned_scenario.interceptor_start_y,
                        state.planned_scenario.interceptor_speed_per_tick,
                        state.planned_scenario.intercept_radius,
                        state.planned_scenario.engagement_timeout_ticks}));
                return;
            }
            if (action_label == "Track") {
                send_and_expect_ack("track_request", icss::protocol::serialize(icss::protocol::TrackRequestPayload{{options.session_id, options.sender_id, state.control.sequence++}, "target-alpha"}));
                return;
            }
            if (action_label == "Activate") {
                send_and_expect_ack("asset_activate", icss::protocol::serialize(icss::protocol::AssetActivatePayload{{options.session_id, options.sender_id, state.control.sequence++}, "asset-interceptor"}));
                return;
            }
            if (action_label == "Command") {
                send_and_expect_ack("command_issue", icss::protocol::serialize(icss::protocol::CommandIssuePayload{{options.session_id, options.sender_id, state.control.sequence++}, "asset-interceptor", "target-alpha"}));
                return;
            }
            if (action_label == "Stop") {
                send_and_expect_ack("scenario_stop", icss::protocol::serialize(icss::protocol::ScenarioStopPayload{{options.session_id, options.sender_id, state.control.sequence++}, "gui control panel stop"}));
                return;
            }
            if (action_label == "Reset") {
                send_and_expect_ack("scenario_reset", icss::protocol::serialize(icss::protocol::ScenarioResetPayload{{options.session_id, options.sender_id, state.control.sequence++}, "gui control panel reset"}));
                state.recent_server_events.clear();
                state.target_history.clear();
                state.asset_history.clear();
                state.last_server_event_tick = 0;
                state.last_server_event_type = "none";
                state.last_server_event_summary = "no server event";
                state.review.visible = false;
                return;
            }
            if (action_label == "Review") {
                if (!review_available(state)) {
                    state.control.last_ok = false;
                    state.control.last_label = "Review";
                    state.control.last_message = "review available after judgment or archive";
                    push_timeline_entry(state, control_timeline_message("Review", false, state.control.last_message));
                    return;
                }
                send_frame(control_socket.fd(),
                           frame_mode,
                           "aar_request",
                           icss::protocol::serialize(icss::protocol::AarRequestPayload{{options.session_id, options.sender_id, state.control.sequence++}, 999U, "absolute"}));
                const auto frame = recv_frame(control_socket.fd(), frame_mode);
                if (frame.kind != "aar_response") {
                    throw std::runtime_error("expected aar_response after Review action");
                }
                const auto response = icss::protocol::parse_aar_response(frame.payload);
                state.control.last_ok = true;
                state.control.last_label = "Review";
                state.control.last_message = "server-side review loaded";
                push_timeline_entry(state, control_timeline_message("Review", true, state.control.last_message));
                state.review.available = true;
                state.review.loaded = true;
                state.review.visible = true;
                state.review.cursor_index = response.replay_cursor_index;
                state.review.total_events = response.total_events;
                state.review.judgment_code = response.judgment_code;
                state.review.resilience_case = response.resilience_case;
                state.review.event_type = response.event_type;
                state.review.event_summary = response.event_summary;
                return;
            }
        };
        while (running) {
            SDL_Event event {};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN && !options.headless) {
                    const int x = event.button.x;
                    const int y = event.button.y;
                    for (const auto& button : build_layout(options).buttons) {
                        if (x >= button.rect.x && x < button.rect.x + button.rect.w
                            && y >= button.rect.y && y < button.rect.y + button.rect.h) {
                            perform_control_action(button.label);
                            break;
                        }
                    }
                }
            }
            const auto now = SDL_GetTicks64();
            if (options.duration_ms > 0 && now - start_ticks >= options.duration_ms) {
                running = false;
            }

#if !defined(_WIN32)
            if (now >= next_heartbeat) {
                const icss::protocol::ViewerHeartbeatPayload heartbeat {
                    {options.session_id, options.sender_id, sequence++},
                    ++state.heartbeat_id,
                };
                send_datagram(socket.get(), server_addr, icss::protocol::serialize(heartbeat));
                ++state.heartbeat_count;
                next_heartbeat = now + options.heartbeat_interval_ms;
            }
            receive_datagrams(socket.get(), state);
            if (options.auto_control_script) {
                static const std::vector<std::string> kDefaultScript {
                    "Start", "Track", "Activate", "Command", "Stop", "Review", "Reset", "Start"
                };
                const auto& script = options.auto_controls.empty() ? kDefaultScript : options.auto_controls;
                if (state.control.auto_step < script.size() && now >= state.control.auto_last_action_ms + 140) {
                    const auto& next_action = script[state.control.auto_step];
                    const bool requires_judgment = (next_action == "Stop" || next_action == "Review")
                        && state.snapshot.command_status != icss::core::CommandLifecycle::None
                        && !state.snapshot.judgment.ready;
                    if (!requires_judgment) {
                        perform_control_action(next_action);
                        state.control.auto_last_action_ms = now;
                        ++state.control.auto_step;
                    }
                }
            }
#endif
            render_gui(renderer, title_font, body_font, state, options);
            SDL_Delay(16);
        }

        write_dump_state(options.dump_state_path, state);
        std::printf("received_snapshot=%s\n", state.received_snapshot ? "true" : "false");
        std::printf("received_telemetry=%s\n", state.received_telemetry ? "true" : "false");
        std::printf("snapshot_sequence=%llu\n", static_cast<unsigned long long>(state.snapshot.header.snapshot_sequence));
        std::printf("connection_state=%s\n", icss::core::to_string(state.snapshot.viewer_connection));
        std::printf("freshness=%s\n", icss::view::freshness_label(state.snapshot).c_str());

        TTF_CloseFont(title_font);
        TTF_CloseFont(body_font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
