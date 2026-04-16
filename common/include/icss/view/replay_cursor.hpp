#pragma once

#include <cstddef>

namespace icss::view {

struct ReplayCursor {
    std::size_t index {0};
    std::size_t total {0};
};

inline constexpr ReplayCursor make_replay_cursor(std::size_t total, std::size_t requested_index) {
    if (total == 0) {
        return {0, 0};
    }
    return {requested_index >= total ? total - 1 : requested_index, total};
}

inline constexpr ReplayCursor step_forward(ReplayCursor cursor) {
    if (cursor.total == 0) {
        return cursor;
    }
    if (cursor.index + 1 < cursor.total) {
        ++cursor.index;
    }
    return cursor;
}

inline constexpr ReplayCursor step_backward(ReplayCursor cursor) {
    if (cursor.index > 0) {
        --cursor.index;
    }
    return cursor;
}

inline constexpr ReplayCursor jump_to(ReplayCursor cursor, std::size_t requested_index) {
    return make_replay_cursor(cursor.total, requested_index);
}

}  // namespace icss::view
