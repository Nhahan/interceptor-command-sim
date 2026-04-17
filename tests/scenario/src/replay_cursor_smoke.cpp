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
    const auto at_end = view::jump_to(cursor, events.size() + 100);
    const auto past_end = view::step_forward(at_end);
    const auto at_start = view::make_replay_cursor(events.size(), 0);
    const auto before_start = view::step_backward(at_start);

    assert(cursor.total == events.size());
    assert(cursor.index == events.size() - 2);
    assert(at_end.index == events.size() - 1);
    assert(past_end.index == at_end.index);
    assert(before_start.index == 0);

    const auto frame = view::render_tactical_frame(snapshot, recent, cursor);
    assert(frame.find("cursor_index=") != std::string::npos);
    assert(frame.find("command_status=") != std::string::npos);
    assert(frame.find("confidence=") != std::string::npos);
    assert(frame.find("freshness=") != std::string::npos);
    assert(frame.find("snapshot_sequence=") != std::string::npos);
    assert(frame.find("packet_loss_pct=") != std::string::npos);
    return 0;
}
