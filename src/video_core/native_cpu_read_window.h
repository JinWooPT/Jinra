// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/alignment.h"
#include "common/common_types.h"

namespace VideoCommon {
struct NativeCpuReadArea {
    u64 first;
    u64 end;
};

class NativeCpuReadWindow {
public:
    NativeCpuReadArea Select(u64 address, u64 size, u64 line_size, u64 page_size) {
        const u64 page = Common::AlignDown(address, page_size);
        NativeCpuReadArea area{page, Common::AlignUp(address + size, page_size)};
        if (size && size <= 8 && line_size < page_size &&
            Common::AlignDown(address + size - 1, page_size) == page) {
            sequential = previous_page == page && address >= previous_end &&
                                 address - previous_end < line_size ? sequential + 1 : 0;
            // Widen linear streams instead of asking the renderer about every cache line.
            if (sequential < 3) {
                area = {Common::AlignDown(address, line_size),
                        Common::AlignUp(address + size, line_size)};
            }
        } else {
            sequential = 0;
        }
        previous_page = page;
        previous_end = area.end;
        return area;
    }

private:
    u64 previous_page{~u64{0}};
    u64 previous_end{};
    unsigned sequential{};
};
} // namespace VideoCommon
