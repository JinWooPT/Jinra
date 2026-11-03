// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include "common/range_sets.inc"

namespace VideoCommon {
// Visit completed snapshots newest first. Earlier snapshots need only write bytes
// that no newer completed snapshot already supplied; CPU-cancelled ranges stay excluded.
class CompletedDownloadCoverage {
public:
    template <typename Callback>
    void ForEachUncovered(DAddr start, u64 size, Callback&& callback) {
        DAddr cursor = start;
        const DAddr end = start + size;
        covered.ForEachInRange(start, size, [&](DAddr newer_start, DAddr newer_end) {
            if (cursor < newer_start) callback(cursor, newer_start);
            cursor = std::max(cursor, newer_end);
        });
        if (cursor < end) callback(cursor, end);
        covered.Add(start, size);
    }
private:
    Common::RangeSet<DAddr> covered;
};
} // namespace VideoCommon
