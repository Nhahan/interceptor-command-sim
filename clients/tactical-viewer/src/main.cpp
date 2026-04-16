#include <iostream>
#include <vector>

#include "icss/core/simulation.hpp"
#include "icss/view/ascii_tactical_view.hpp"

int main() {
    using namespace icss::core;
    namespace view = icss::view;

    auto session = run_baseline_demo();
    const auto& events = session.events();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    std::vector<EventRecord> recent(events.begin() + static_cast<std::ptrdiff_t>(start), events.end());

    std::cout << view::render_tactical_frame(session.latest_snapshot(), recent, events.size() - 1);
    return 0;
}
