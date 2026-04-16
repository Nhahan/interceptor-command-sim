#include <cassert>
#include <string>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/view/ascii_tactical_view.hpp"
#include "icss/view/replay_cursor.hpp"

int main() {
    using namespace icss::core;
    namespace view = icss::view;

    auto session = run_baseline_demo();
    const auto snapshot = session.latest_snapshot();
    const auto events = session.events();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    std::vector<EventRecord> recent(events.begin() + static_cast<std::ptrdiff_t>(start), events.end());

    auto cursor = view::make_replay_cursor(events.size(), 0);
    cursor = view::step_forward(cursor);
    cursor = view::jump_to(cursor, events.size() - 1);
    cursor = view::step_backward(cursor);

    assert(cursor.total == events.size());
    assert(cursor.index == events.size() - 2);

    const auto frame = view::render_tactical_frame(snapshot, recent, cursor);
    assert(frame.find("cursor_index=") != std::string::npos);
    assert(frame.find("command_status=") != std::string::npos);
    assert(frame.find("confidence=") != std::string::npos);
    return 0;
}
