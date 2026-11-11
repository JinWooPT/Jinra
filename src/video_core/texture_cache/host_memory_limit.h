// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <limits>

#include "common/common_types.h"
#include "common/literals.h"

namespace VideoCommon {

/// Retain room for the OS, guest memory and other allocations without evicting all textures.
[[nodiscard]] constexpr u64 HostCriticalMemory(u64 device_usage, u64 commit_room) {
    using namespace Common::Literals;
    const u64 spare = commit_room - (std::min)(commit_room, 2_GiB);
    const u64 room = (std::min)(spare, std::numeric_limits<u64>::max() - device_usage);
    return (std::max)(3_GiB, device_usage + room);
}

} // namespace VideoCommon
