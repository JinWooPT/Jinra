// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <span>
#include "common/common_types.h"

namespace VideoCommon {
inline bool PackedPagesMatch(std::span<const u64> entries, size_t first, size_t count, u64 kind) {
    if (!count) return true;
    if (kind > 3 || first >= entries.size() * 32 || count > entries.size() * 32 - first) {
        return false;
    }
    constexpr u64 low_bits = 0x5555555555555555ULL;
    while (count) {
        const size_t offset = first % 32;
        const size_t pages = std::min(count, 32 - offset);
        const u64 mask = (~u64{0} >> (64 - pages * 2)) << (offset * 2);
        if ((entries[first / 32] & mask) != ((low_bits * kind) & mask)) return false;
        first += pages;
        count -= pages;
    }
    return true;
}

// Small pages accept reserved and mapped entries (01 and 10). Free (00) and
// unknown (11) entries retain the old probe's unmapped behavior.
inline bool PackedPagesPresent(std::span<const u64> entries, size_t first, size_t count) {
    if (!count) return true;
    if (first >= entries.size() * 32 || count > entries.size() * 32 - first) return false;
    constexpr u64 low_bits = 0x5555555555555555ULL;
    while (count) {
        const size_t offset = first % 32;
        const size_t pages = std::min(count, 32 - offset);
        const u64 mask = ((~u64{0} >> (64 - pages * 2)) << (offset * 2)) & low_bits;
        const u64 word = entries[first / 32];
        if ((((word ^ (word >> 1)) & low_bits) & mask) != mask) return false;
        first += pages;
        count -= pages;
    }
    return true;
}

inline bool PackedGpuRangeMapped(std::span<const u64> big_entries,
                                 std::span<const u64> small_entries, unsigned big_bits,
                                 unsigned small_bits, u64 address, u64 size) {
    if (!size) return true;
    if (big_bits >= 64 || small_bits > big_bits || size - 1 > ~u64{0} - address) return false;
    const size_t first_big = address >> big_bits;
    const size_t big_count = ((address + size - 1) >> big_bits) - first_big + 1;
    if (first_big >= big_entries.size() * 32 || big_count > big_entries.size() * 32 - first_big) {
        return false;
    }
    if (PackedPagesMatch(big_entries, first_big, big_count, 2)) return true;
    const auto small_present = [&](u64 first, u64 bytes) {
        const size_t page = first >> small_bits;
        return PackedPagesPresent(small_entries, page,
                                  ((first + bytes - 1) >> small_bits) - page + 1);
    };
    if (PackedPagesMatch(big_entries, first_big, big_count, 0)) {
        return small_present(address, size);
    }
    const u64 big_mask = (u64{1} << big_bits) - 1;
    while (size) {
        const size_t page = address >> big_bits;
        const u64 kind = (big_entries[page / 32] >> ((page % 32) * 2)) & 3;
        const u64 bytes = std::min((u64{1} << big_bits) - (address & big_mask), size);
        if (kind == 1 || (kind != 2 && !small_present(address, bytes))) return false;
        address += bytes;
        size -= bytes;
    }
    return true;
}
} // namespace VideoCommon
