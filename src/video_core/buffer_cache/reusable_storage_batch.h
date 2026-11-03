// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace VideoCommon {
// Keep entry-owned allocations while excluding every entry from the preceding draw.
template <typename Entry>
class ReusableStorageBatch {
public:
    void Reset(bool retain) {
        active = 0;
        if (!retain) entries.clear();
    }
    std::span<Entry> Active() { return {entries.data(), active}; }
    Entry& Append() {
        if (active == entries.size()) entries.emplace_back();
        return entries[active++];
    }
private:
    std::vector<Entry> entries;
    std::size_t active{};
};
} // namespace VideoCommon
