#pragma once

#include <string>
#include <vector>

#include "icss/core/types.hpp"

namespace icss::view {

std::string render_tactical_frame(const icss::core::Snapshot& snapshot,
                                  const std::vector<icss::core::EventRecord>& recent_events,
                                  std::size_t aar_cursor_index);

}  // namespace icss::view
