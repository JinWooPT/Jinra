// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include "common/common_types.h"

namespace VideoCommon {

class UsageTracker {
    // PAGE_SHIFT is a macro on FreeBSD
    static constexpr size_t BUFFER_BYTES_PER_BITSHIFT = 6;
    static constexpr size_t BUFFER_PAGE_SHIFT = 6 + BUFFER_BYTES_PER_BITSHIFT;
    static constexpr size_t BUFFER_PAGE_BYTES = 1 << BUFFER_PAGE_SHIFT;

public:
    explicit UsageTracker(size_t size, bool merge_ranges_ = false)
        : size_bytes{size}, merge_ranges{merge_ranges_} {
        const size_t num_pages = (size >> BUFFER_PAGE_SHIFT) + 1;
        pages.resize(num_pages, 0ULL);
    }

    void Reset() noexcept {
        std::ranges::fill(pages, 0ULL);
        recent.fill({~u64{0}, 0});
        recent_cursor = 0;
    }

    void Track(u64 offset, u64 size) noexcept {
        if (!size || offset > size_bytes || size > size_bytes - offset) {
            return;
        }
        const u64 end = offset + size;
        if (merge_ranges) {
            for (auto& [recent_offset, recent_size] : recent) {
                if (!recent_size) {
                    continue;
                }
                const u64 recent_end = recent_offset + recent_size;
                if (offset > recent_end || recent_offset > end) {
                    continue;
                }
                if (offset < recent_offset) {
                    TrackRange(offset, recent_offset - offset);
                }
                if (end > recent_end) {
                    TrackRange(recent_end, end - recent_end);
                }
                const u64 first = std::min(offset, recent_offset);
                recent_size = std::max(end, recent_end) - first;
                recent_offset = first;
                return;
            }
        }
        // Tracking is idempotent until Reset: skip ranges tracked just before. Draws bind the
        // same (often multi-megabyte) storage buffers again and again, and each Track wrote a
        // word per 4 KiB of the range.
        for (const auto& [recent_offset, recent_size] : recent) {
            if (recent_offset == offset && recent_size == size) {
                return;
            }
        }
        recent[recent_cursor++ % recent.size()] = {offset, size};
        TrackRange(offset, size);
    }

    [[nodiscard]] bool IsUsed(u64 offset, u64 size) const noexcept {
        if (!size) {
            return false;
        }
        if (offset > size_bytes || size > size_bytes - offset) {
            return true;
        }
        const size_t page = offset >> BUFFER_PAGE_SHIFT;
        const size_t last_page = (offset + size - 1) >> BUFFER_PAGE_SHIFT;
        if ((pages[page] & PageMask(offset, size)) != 0) {
            return true;
        }
        if (page == last_page) {
            return false;
        }
        for (size_t i = page + 1; i < last_page; ++i) {
            if (pages[i] != 0) {
                return true;
            }
        }
        const u64 last_start = static_cast<u64>(last_page) << BUFFER_PAGE_SHIFT;
        return (pages[last_page] & PageMask(last_start, offset + size - last_start)) != 0;
    }

private:
    void TrackRange(u64 offset, u64 size) noexcept {
        const size_t page = offset >> BUFFER_PAGE_SHIFT;
        const size_t last_page = (offset + size - 1) >> BUFFER_PAGE_SHIFT;
        pages[page] |= PageMask(offset, size);
        if (page == last_page) {
            return;
        }
        std::fill(pages.begin() + page + 1, pages.begin() + last_page, ~u64{0});
        const u64 last_start = static_cast<u64>(last_page) << BUFFER_PAGE_SHIFT;
        pages[last_page] |= PageMask(last_start, offset + size - last_start);
    }

    static u64 PageMask(u64 offset, u64 size) noexcept {
        const u64 first = offset % BUFFER_PAGE_BYTES;
        const u64 end = first + std::min<u64>(size, BUFFER_PAGE_BYTES - first);
        const u64 first_bit = first >> BUFFER_BYTES_PER_BITSHIFT;
        const u64 last_bit = (end - 1) >> BUFFER_BYTES_PER_BITSHIFT;
        return (~u64{0} >> (63 - (last_bit - first_bit))) << first_bit;
    }

private:
    std::vector<u64> pages;
    /// Ranges tracked last, (offset, size).
    std::array<std::pair<u64, u64>, 4> recent{{{~u64{0}, 0}, {~u64{0}, 0}, {~u64{0}, 0}, {~u64{0}, 0}}};
    size_t recent_cursor{};
    u64 size_bytes{};
    bool merge_ranges{};
};

} // namespace VideoCommon
