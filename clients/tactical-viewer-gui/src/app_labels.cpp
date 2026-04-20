#include "app.hpp"

#include <string>
#include <vector>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::viewer_gui {

icss::core::InterceptorStatus parse_interceptor_status(std::string_view value) {
    if (value == "idle") return icss::core::InterceptorStatus::Idle;
    if (value == "ready") return icss::core::InterceptorStatus::Ready;
    if (value == "intercepting") return icss::core::InterceptorStatus::Intercepting;
    if (value == "complete") return icss::core::InterceptorStatus::Complete;
    return icss::core::InterceptorStatus::Idle;
}

icss::core::EngageOrderStatus parse_engage_order_status(std::string_view value) {
    if (value == "none") return icss::core::EngageOrderStatus::None;
    if (value == "accepted") return icss::core::EngageOrderStatus::Accepted;
    if (value == "executing") return icss::core::EngageOrderStatus::Executing;
    if (value == "completed") return icss::core::EngageOrderStatus::Completed;
    if (value == "rejected") return icss::core::EngageOrderStatus::Rejected;
    return icss::core::EngageOrderStatus::None;
}

icss::core::AssessmentCode parse_assessment_code(std::string_view value) {
    if (value == "intercept_success") return icss::core::AssessmentCode::InterceptSuccess;
    if (value == "invalid_transition") return icss::core::AssessmentCode::InvalidTransition;
    if (value == "timeout_observed") return icss::core::AssessmentCode::TimeoutObserved;
    return icss::core::AssessmentCode::Pending;
}

icss::core::ConnectionState parse_connection_state(std::string_view value) {
    if (value == "connected") return icss::core::ConnectionState::Connected;
    if (value == "reconnected") return icss::core::ConnectionState::Reconnected;
    if (value == "timed_out") return icss::core::ConnectionState::TimedOut;
    return icss::core::ConnectionState::Disconnected;
}

icss::core::SessionPhase parse_phase(std::string_view value) {
    if (value == "standby") return icss::core::SessionPhase::Standby;
    if (value == "detecting") return icss::core::SessionPhase::Detecting;
    if (value == "tracking") return icss::core::SessionPhase::Tracking;
    if (value == "interceptor_ready") return icss::core::SessionPhase::InterceptorReady;
    if (value == "engage_ordered") return icss::core::SessionPhase::EngageOrdered;
    if (value == "intercepting") return icss::core::SessionPhase::Intercepting;
    if (value == "assessed") return icss::core::SessionPhase::Assessed;
    if (value == "archived") return icss::core::SessionPhase::Archived;
    return icss::core::SessionPhase::Standby;
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
    case icss::core::SessionPhase::Standby: return "STANDBY";
    case icss::core::SessionPhase::Detecting: return "DETECTING";
    case icss::core::SessionPhase::Tracking: return "TRACK ESTABLISHED";
    case icss::core::SessionPhase::InterceptorReady: return "INTERCEPTOR READY";
    case icss::core::SessionPhase::EngageOrdered: return "ENGAGE ORDERED";
    case icss::core::SessionPhase::Intercepting: return "INTERCEPTING";
    case icss::core::SessionPhase::Assessed: return "ASSESSMENT COMPLETE";
    case icss::core::SessionPhase::Archived: return "ARCHIVED";
    }
    return "UNKNOWN";
}

std::string phase_operator_note(icss::core::SessionPhase phase) {
    switch (phase) {
    case icss::core::SessionPhase::Standby:
        return "Ready to initialize the intercept scenario.";
    case icss::core::SessionPhase::Detecting:
        return "Target detected. Acquire track to build a firing solution.";
    case icss::core::SessionPhase::Tracking:
        return "Track established. Ready the interceptor.";
    case icss::core::SessionPhase::InterceptorReady:
        return "Interceptor ready. Issue engage order when authorized.";
    case icss::core::SessionPhase::EngageOrdered:
        return "Engage order accepted by the authoritative server.";
    case icss::core::SessionPhase::Intercepting:
        return "Interceptor in flight. Awaiting server assessment.";
    case icss::core::SessionPhase::Assessed:
        return "Assessment complete. Open AAR or reset the run.";
    case icss::core::SessionPhase::Archived:
        return "Session archived. Reset to begin a new run.";
    }
    return "Unknown phase.";
}

SDL_Color phase_accent(icss::core::SessionPhase phase) {
    switch (phase) {
    case icss::core::SessionPhase::Standby: return SDL_Color {143, 157, 179, 255};
    case icss::core::SessionPhase::Detecting: return SDL_Color {244, 180, 0, 255};
    case icss::core::SessionPhase::Tracking: return SDL_Color {80, 200, 120, 255};
    case icss::core::SessionPhase::InterceptorReady: return SDL_Color {77, 171, 247, 255};
    case icss::core::SessionPhase::EngageOrdered: return SDL_Color {255, 149, 0, 255};
    case icss::core::SessionPhase::Intercepting: return SDL_Color {255, 99, 71, 255};
    case icss::core::SessionPhase::Assessed: return SDL_Color {179, 136, 255, 255};
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
    if (state.snapshot.assessment.ready) {
        return "ASSESSMENT: " + uppercase_words(icss::core::to_string(state.snapshot.assessment.code));
    }
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return "REJECTED CONTROL: " + uppercase_words(state.control.last_label);
    }
    if (state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing) {
        return "INTERCEPT IN PROGRESS";
    }
    if (state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Accepted
        || state.snapshot.phase == icss::core::SessionPhase::EngageOrdered) {
        return "ENGAGE ORDER ACCEPTED";
    }
    if (!state.recent_server_events.empty()) {
        return "LAST EVENT: " + uppercase_words(state.last_server_event_type);
    }
    return "AWAITING AUTHORITATIVE EVENT";
}

std::string authoritative_badge_label(const ViewerState& state) {
    if (state.snapshot.assessment.ready) {
        return "ASSESSED";
    }
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return "REJECTED";
    }
    if (state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing) {
        return "INTERCEPTING";
    }
    if (state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Accepted
        || state.snapshot.phase == icss::core::SessionPhase::EngageOrdered) {
        return "ORDERED";
    }
    if (!state.recent_server_events.empty()) {
        return "LIVE";
    }
    return "AWAITING";
}

SDL_Color authoritative_badge_color(const ViewerState& state) {
    if (state.snapshot.assessment.ready) {
        return SDL_Color {179, 136, 255, 255};
    }
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return SDL_Color {255, 99, 120, 255};
    }
    if (state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Executing) {
        return SDL_Color {255, 149, 0, 255};
    }
    if (state.snapshot.engage_order_status == icss::core::EngageOrderStatus::Accepted
        || state.snapshot.phase == icss::core::SessionPhase::EngageOrdered) {
        return SDL_Color {255, 201, 107, 255};
    }
    if (!state.recent_server_events.empty()) {
        return SDL_Color {120, 190, 255, 255};
    }
    return SDL_Color {143, 157, 179, 255};
}

bool aar_available(const ViewerState& state) {
    return state.snapshot.phase == icss::core::SessionPhase::Assessed
        || state.snapshot.phase == icss::core::SessionPhase::Archived
        || state.snapshot.assessment.ready;
}

std::string aar_panel_text(const ViewerState& state) {
    if (!aar_available(state)) {
        return "AAR unavailable until authoritative assessment or archive.\n"
               "Use the live log during active control.";
    }
    if (!state.aar.loaded) {
        return "Server-side AAR is available.\n"
               "Press AAR to load cursor, assessment, resilience, and event summary.";
    }

    std::string text;
    text += "AAR cursor: " + std::to_string(state.aar.cursor_index) + "/" + std::to_string(state.aar.total_events) + "\n";
    text += "Assessment: " + state.aar.assessment_code + "\n";
    text += "Resilience: " + state.aar.resilience_case + "\n";
    text += "Current event: " + state.aar.event_type + "\n";
    text += "Summary: " + state.aar.event_summary;
    return text;
}

std::string control_display_label(std::string_view action, const ViewerState& state) {
    if (action == "Track") {
        return state.snapshot.track.active ? "Drop Track" : "Acquire Track";
    }
    if (action == "Ready") {
        return "Ready";
    }
    if (action == "Engage") {
        return "Engage";
    }
    if (action == "AAR") {
        return "AAR";
    }
    return std::string(action);
}

std::string terminal_timeline_text(const ViewerState& state, bool aar_mode) {
    std::string out;
    out += aar_mode ? "mode=aar | source=server_aar\n" : "mode=live | source=server_events\n";
    out += "----------------------------------------\n";
    if (aar_mode) {
        const auto body = aar_panel_text(state);
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

std::vector<std::string> terminal_timeline_lines(const ViewerState& state, bool aar_mode) {
    std::vector<std::string> lines;
    const auto text = terminal_timeline_text(state, aar_mode);
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
