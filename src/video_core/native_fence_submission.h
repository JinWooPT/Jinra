// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdlib>
#include "video_core/nx2.h"

namespace VideoCommon {
inline bool NativeFenceSubmissionDeferred() {
    static const bool requested = [] {
        const char* value = std::getenv("EDEN_NATIVE_DEFER_FENCE_SUBMIT");
        return value && value[0] == '1';
    }();
    return requested && Tegra::g_native_channels.load(std::memory_order_relaxed);
}
} // namespace VideoCommon
