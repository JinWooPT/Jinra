// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "common/container/unordered_map.h"
#include "common/range_sets.inc"

namespace VideoCommon {
inline bool NativeSelectivePrefixCacheEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("EDEN_NATIVE_SELECTIVE_PREFIX_CACHE");
        return value && value[0] == '1';
    }();
    return enabled;
}

inline bool NativePrefixIntervalsEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("EDEN_NATIVE_PREFIX_INTERVALS");
        return value && value[0] == '1';
    }();
    return enabled;
}

class MappedPrefixCache {
public:
    static constexpr size_t MAX_ENTRIES = 4096;
    static constexpr size_t MAX_DEFERRED = 128;

    explicit MappedPrefixCache(bool cache_intervals = false) : use_intervals{cache_intervals} {}

    template <class Probe>
    u32 Query(size_t as_id, GPUVAddr address, u32 size, Probe&& probe) {
        if (pending.load(std::memory_order_acquire)) ApplyInvalidations();
        if (size > std::numeric_limits<GPUVAddr>::max() - address) {
            return probe(address, size);
        }
        const u64 key = (static_cast<u64>(as_id) << 48) ^ address;
        const auto found = entries.find(key);
        if (found != entries.end() && found->second.as_id == as_id &&
            found->second.address == address && found->second.size == size) {
            ++hits;
            return found->second.prefix;
        }
        u32 prefix = 0;
        if (use_intervals) {
            known_mapped[as_id].ForEachInRange(address, size, [&](GPUVAddr first, GPUVAddr end) {
                if (first == address) prefix = static_cast<u32>(end - address);
            });
        }
        if (prefix == size) {
            ++range_hits;
        } else {
            ++walks;
            prefix += probe(address + prefix, size - prefix);
            if (use_intervals && prefix) known_mapped[as_id].Add(address, prefix);
        }
        if (entries.size() >= MAX_ENTRIES) entries.clear();
        entries.insert_or_assign(key, Entry{as_id, address, size, prefix});
        return prefix;
    }

    // Mapping callbacks may arrive from another thread. Apply them on the cache owner.
    void Invalidate(size_t as_id, GPUVAddr address, u64 size) {
        if (!size) return;
        std::scoped_lock lock{mutex};
        if (invalidations.size() == MAX_DEFERRED) {
            invalidations.clear();
            overflow = true;
        }
        invalidations.push_back({as_id, address, size});
        pending.store(true, std::memory_order_release);
    }

    u64 Hits() const { return hits; }
    u64 Walks() const { return walks; }
    u64 Removed() const { return removed; }
    u64 RangeHits() const { return range_hits; }

private:
    struct Entry { size_t as_id; GPUVAddr address; u32 size; u32 prefix; };
    struct Invalidation { size_t as_id; GPUVAddr address; u64 size; };

    static bool Overlaps(GPUVAddr a, u64 a_size, GPUVAddr b, u64 b_size) {
        if (!a_size || !b_size) return false;
        return a <= b ? b - a < a_size : a - b < b_size;
    }

    void ApplyInvalidations() {
        std::vector<Invalidation> changes;
        bool clear;
        {
            std::scoped_lock lock{mutex};
            pending.store(false, std::memory_order_relaxed);
            changes.swap(invalidations);
            clear = std::exchange(overflow, false);
        }
        if (clear) {
            removed += entries.size();
            entries.clear();
            known_mapped.clear();
            return;
        }
        if (use_intervals) {
            for (const auto& change : changes) {
                const auto known = known_mapped.find(change.as_id);
                if (known == known_mapped.end()) continue;
                if (change.size > std::numeric_limits<GPUVAddr>::max() - change.address) {
                    known->second.Clear();
                } else {
                    known->second.Subtract(change.address, change.size);
                }
            }
        }
        for (auto it = entries.begin(); it != entries.end();) {
            const auto& entry = it->second;
            bool stale = false;
            for (const auto& change : changes) {
                stale |= entry.as_id == change.as_id &&
                    Overlaps(entry.address, entry.size, change.address, change.size);
                if (stale) break;
            }
            if (stale) {
                ++removed;
                const auto old = it++;
                entries.erase(old);
            } else {
                ++it;
            }
        }
    }

    Common::unordered_map<u64, Entry> entries;
    Common::unordered_map<size_t, Common::RangeSet<GPUVAddr>> known_mapped;
    const bool use_intervals;
    std::mutex mutex;
    std::vector<Invalidation> invalidations;
    std::atomic<bool> pending{};
    bool overflow{};
    u64 hits{}, walks{}, removed{}, range_hits{};
};
} // namespace VideoCommon
