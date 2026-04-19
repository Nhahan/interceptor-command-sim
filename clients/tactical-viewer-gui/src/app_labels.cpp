#include "app.hpp"

#include <string>
#include <vector>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::viewer_gui {

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
    case icss::core::SessionPhase::Tracking: return "GUIDANCE LOCKED";
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
        return "Target is present. Awaiting guidance enable.";
    case icss::core::SessionPhase::Tracking:
        return "Guidance enabled. Interceptor can be prepared.";
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
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Executing) {
        return "ACTIVE COMMAND: INTERCEPTOR ENGAGING";
    }
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Accepted
        || state.snapshot.phase == icss::core::SessionPhase::CommandIssued) {
        return "COMMAND ACCEPTED: AWAITING LIVE ENGAGEMENT";
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
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Executing) {
        return "ENGAGING";
    }
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Accepted
        || state.snapshot.phase == icss::core::SessionPhase::CommandIssued) {
        return "ACCEPTED";
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
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Executing) {
        return SDL_Color {255, 149, 0, 255};
    }
    if (state.snapshot.command_status == icss::core::CommandLifecycle::Accepted
        || state.snapshot.phase == icss::core::SessionPhase::CommandIssued) {
        return SDL_Color {255, 201, 107, 255};
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

std::vector<std::string> terminal_timeline_lines(const ViewerState& state, bool review_mode) {
    std::vector<std::string> lines;
    const auto text = terminal_timeline_text(state, review_mode);
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
        if (start == text.size()) {
            break;
        }
    }
    return lines;
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

}  // namespace icss::viewer_gui
