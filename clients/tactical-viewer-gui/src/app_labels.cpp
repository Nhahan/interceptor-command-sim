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
    case icss::core::SessionPhase::Tracking: return "TRACK FILE";
    case icss::core::SessionPhase::InterceptorReady: return "WEAPON READY";
    case icss::core::SessionPhase::EngageOrdered: return "FIRE ORDERED";
    case icss::core::SessionPhase::Intercepting: return "INTERCEPTING";
    case icss::core::SessionPhase::Assessed: return "ENGAGEMENT ASSESSED";
    case icss::core::SessionPhase::Archived: return "ARCHIVED";
    }
    return "UNKNOWN";
}

std::string phase_operator_note(icss::core::SessionPhase phase) {
    switch (phase) {
    case icss::core::SessionPhase::Standby:
        return "Ready to initialize the intercept scenario.";
    case icss::core::SessionPhase::Detecting:
        return "Target detected. Build the track file before weapon preparation.";
    case icss::core::SessionPhase::Tracking:
        return "Track file valid. Ready the weapon.";
    case icss::core::SessionPhase::InterceptorReady:
        return "Weapon ready. Issue fire order, or drop track before launch.";
    case icss::core::SessionPhase::EngageOrdered:
        return "Fire order accepted by the authoritative server.";
    case icss::core::SessionPhase::Intercepting:
        return "Weapon in flight. Awaiting engagement assessment.";
    case icss::core::SessionPhase::Assessed:
        return "Engagement assessed. Open Review or reset the run.";
    case icss::core::SessionPhase::Archived:
        return "Run archived. Open Review or reset to begin a new run.";
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
    if (freshness == "reacquiring") {
        return "Link restored. The tactical picture is rebuilding from authoritative updates.";
    }
    if (freshness == "degraded") {
        return "Picture degraded. Packet loss is present, but the feed is still updating.";
    }
    if (freshness == "stale") {
        return "Picture stale. Treat geometry and timing as low-confidence data.";
    }
    return "Picture current. Feed is aligned with authoritative snapshots.";
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
        return "Post-engagement review unavailable until assessment or archive.\n"
               "Use the live event log during active control.";
    }
    if (!state.aar.loaded) {
        return "Post-engagement review available.\n"
               "Press Review to load the authoritative review cursor and event summary.";
    }

    std::string text;
    text += "Review cursor: " + std::to_string(state.aar.cursor_index) + "/" + std::to_string(state.aar.total_events) + "\n";
    text += "Assessment: " + state.aar.assessment_code + "\n";
    text += "Link/Picture: " + state.aar.resilience_case + "\n";
    text += "Current record: " + state.aar.event_type + "\n";
    text += "Summary: " + state.aar.event_summary;
    return text;
}

std::string control_display_label(std::string_view action, const ViewerState& state) {
    if (action == "Track") {
        if (state.snapshot.phase == icss::core::SessionPhase::EngageOrdered
            || state.snapshot.phase == icss::core::SessionPhase::Intercepting
            || state.snapshot.phase == icss::core::SessionPhase::Assessed
            || state.snapshot.phase == icss::core::SessionPhase::Archived) {
            return "Locked";
        }
        return state.snapshot.track.active ? "Drop" : "Acquire";
    }
    if (action == "Ready") {
        return "Ready";
    }
    if (action == "Engage") {
        return "Engage";
    }
    if (action == "AAR") {
        return "Review";
    }
    return std::string(action);
}

bool control_button_enabled(std::string_view action, const ViewerState& state) {
    if (action == "Start") {
        return state.snapshot.phase == icss::core::SessionPhase::Standby;
    }
    if (action == "Track") {
        return state.snapshot.phase == icss::core::SessionPhase::Detecting
            || state.snapshot.phase == icss::core::SessionPhase::Tracking
            || state.snapshot.phase == icss::core::SessionPhase::InterceptorReady;
    }
    if (action == "Ready") {
        return state.snapshot.phase == icss::core::SessionPhase::Tracking
            && state.snapshot.track.active;
    }
    if (action == "Engage") {
        return state.snapshot.phase == icss::core::SessionPhase::InterceptorReady
            && state.snapshot.interceptor_status == icss::core::InterceptorStatus::Ready;
    }
    if (action == "Reset") {
        return state.snapshot.phase != icss::core::SessionPhase::Standby
            || state.received_snapshot
            || state.control.last_label != "idle";
    }
    if (action == "AAR") {
        return aar_available(state);
    }
    return true;
}

std::string control_panel_hint(const ViewerState& state) {
    if (!state.control.last_ok && state.control.last_label != "idle") {
        return "Last command was rejected: " + state.control.last_message;
    }
    switch (state.snapshot.phase) {
    case icss::core::SessionPhase::Standby:
        return "Start opens a new run. The rest unlock as the phase advances.";
    case icss::core::SessionPhase::Detecting:
        return "Acquire Track first. Engage stays disabled until Ready succeeds.";
    case icss::core::SessionPhase::Tracking:
        return "Track is live. Ready the interceptor before launch.";
    case icss::core::SessionPhase::InterceptorReady:
        if (state.snapshot.track.active) {
            return "Ready to launch. Engage fires now; Track can still drop the lock.";
        }
        return "Unguided launch is possible. Reacquire Track for a guided intercept.";
    case icss::core::SessionPhase::EngageOrdered:
    case icss::core::SessionPhase::Intercepting:
        return "The interceptor is committed. Controls stay locked until assessment.";
    case icss::core::SessionPhase::Assessed:
        return "Assessment complete. Open the post-engagement review or reset the run.";
    case icss::core::SessionPhase::Archived:
        if (state.aar.loaded) {
            return "Post-engagement review loaded. Reset to start another run.";
        }
        return "Run archived. Open Review or reset for another iteration.";
    }
    return "Control availability follows the authoritative mission phase.";
}

std::string terminal_timeline_text(const ViewerState& state, bool aar_mode) {
    std::string out;
    out += aar_mode ? "mode=post_engagement_review | source=authoritative_history\n"
                    : "mode=live_tactical_feed | source=authoritative_events\n";
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
