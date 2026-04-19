#include "app.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace icss::viewer_gui {

void push_timeline_entry(ViewerState& state, std::string message) {
    if (!state.recent_server_events.empty() && state.recent_server_events.front() == message) {
        return;
    }
    if (state.timeline_scroll_lines > 0) {
        ++state.timeline_scroll_lines;
    }
    state.recent_server_events.push_front(std::move(message));
    while (state.recent_server_events.size() > 64) {
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

std::string format_fixed_1(float value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

std::string format_fixed_0(std::uint32_t value) {
    return std::to_string(value);
}

float heading_deg_gui(icss::core::Vec2f value) {
    if (std::abs(value.x) <= 1e-5F && std::abs(value.y) <= 1e-5F) {
        return 0.0F;
    }
    return std::atan2(value.y, value.x) * 180.0F / 3.1415926535F;
}

}  // namespace icss::viewer_gui
