// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include "video_core/nx2.h"

namespace VideoCommon {
inline bool NativeReactiveReadbackEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("EDEN_NATIVE_REACTIVE_BUFFER_READS");
        return !value || value[0] != '0';
    }();
    return enabled && Tegra::g_native_channels.load(std::memory_order_relaxed);
}

// GPU writes invalidate CPU hints before their next read; cached ranges alone
// cannot suppress readback when background buffer downloads are disabled.
class NativeReadbackEpoch {
public:
    std::uint64_t Observe() const { return epoch.load(std::memory_order_acquire); }
    void Invalidate() { epoch.fetch_add(1, std::memory_order_release); }

private:
    std::atomic<std::uint64_t> epoch{1};
};

inline NativeReadbackEpoch native_readback_epoch;

class NativeReadbackHint {
public:
    bool Contains(std::uint64_t address, std::uint64_t size, std::uint64_t observed) const {
        return observed == epoch && address >= first && address <= end && size <= end - address;
    }
    void Remember(std::uint64_t first_, std::uint64_t end_, std::uint64_t observed) {
        first = first_;
        end = end_;
        epoch = observed;
    }

private:
    std::uint64_t first{}, end{}, epoch{};
};
} // namespace VideoCommon
