// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <span>
#include <utility>

#include "video_core/texture_cache/types.h"

namespace VideoCommon {

inline constexpr size_t DOWNLOAD_COPY_BATCH_SIZE = 256;

// Preserve copy order and staging offsets while sharing one command for consecutive
// regions from the same source. The callback consumes the temporary span immediately.
template <typename BufferId, typename Callback>
void ForEachDownloadCopyBatch(std::span<const std::pair<BufferCopy, BufferId>> downloads,
                              Callback&& callback) {
    std::array<BufferCopy, DOWNLOAD_COPY_BATCH_SIZE> copies;
    size_t first = 0;
    while (first < downloads.size()) {
        const BufferId buffer_id = downloads[first].second;
        size_t count = 0;
        do {
            copies[count] = downloads[first + count].first;
            ++count;
        } while (count < copies.size() && first + count < downloads.size() &&
                 downloads[first + count].second == buffer_id);
        callback(buffer_id, std::span<const BufferCopy>{copies.data(), count});
        first += count;
    }
}

} // namespace VideoCommon
