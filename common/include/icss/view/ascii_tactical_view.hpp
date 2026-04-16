#pragma once

#include <string>
#include <vector>

#include "icss/core/types.hpp"
#include "icss/view/replay_cursor.hpp"

namespace icss::view {

std::string render_tactical_frame(const icss::core::Snapshot& snapshot,
                                  const std::vector<icss::core::EventRecord>& recent_events,
                                  ReplayCursor cursor);

}  // namespace icss::view
