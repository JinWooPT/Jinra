// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <limits>
#include "common/common_types.h"

namespace Common::InputReplay {
inline const bool frame_clock = [] {
    const char* value = std::getenv("EDEN_AUTO_FRAME_CLOCK");
    return value && value[0] == '1';
}();
inline const auto epoch = std::chrono::steady_clock::now();
inline std::atomic<u64> frames{};
inline std::atomic<u64> first_frame{std::numeric_limits<u64>::max()};

inline void EndGameFrame() {
    if (frame_clock) frames.fetch_add(1, std::memory_order_relaxed);
}

// Menus use wall time; after loading, replay identical inputs per presented game frame.
inline double Time() {
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - epoch).count();
    if (!frame_clock || wall < 50.0) return wall;
    const u64 current = frames.load(std::memory_order_relaxed);
    u64 base = std::numeric_limits<u64>::max();
    if (first_frame.compare_exchange_strong(base, current, std::memory_order_relaxed)) base = current;
    return 50.0 + static_cast<double>(current >= base ? current - base : 0) / 60.0;
}
} // namespace Common::InputReplay
