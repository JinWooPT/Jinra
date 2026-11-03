// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdlib>
#include <utility>

namespace VideoCommon {
enum class NativeReadbackReason : unsigned { Other, GuestRead, PreserveWrite, Count };
inline const bool track_native_readback_reasons =
    std::getenv("EDEN_NATIVE_SYNC_READBACK_STATS") != nullptr;
inline thread_local NativeReadbackReason native_readback_reason{NativeReadbackReason::Other};

class NativeReadbackScope {
public:
    explicit NativeReadbackScope(NativeReadbackReason reason) {
        if (track_native_readback_reasons) {
            previous = std::exchange(native_readback_reason, reason);
        }
    }
    ~NativeReadbackScope() {
        if (track_native_readback_reasons) {
            native_readback_reason = previous;
        }
    }

private:
    NativeReadbackReason previous{NativeReadbackReason::Other};
};
} // namespace VideoCommon
