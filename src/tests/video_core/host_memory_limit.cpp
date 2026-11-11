// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "video_core/texture_cache/host_memory_limit.h"

TEST_CASE("Texture cache reserves host commit headroom", "[video_core][host-memory-limit]") {
    using namespace Common::Literals;
    using VideoCommon::HostCriticalMemory;

    CHECK(HostCriticalMemory(4_GiB, 6_GiB) == 8_GiB);
    CHECK(HostCriticalMemory(3_GiB, 3_GiB) == 4_GiB);
    CHECK(HostCriticalMemory(2_GiB, 0) == 3_GiB);
    CHECK(HostCriticalMemory(std::numeric_limits<u64>::max(), 4_GiB) ==
          std::numeric_limits<u64>::max());
}
