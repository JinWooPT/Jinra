// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <fmt/format.h>

#include "common/logging.h"
#include "video_core/nx2_stutter_trace.h"

namespace VideoCommon::StutterTrace {
namespace {

// Per-frame tracing from Lichii #975, adapted to Jinra's prepared SPIR-V and Vulkan waits.
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr size_t CategoryCount = static_cast<size_t>(Category::Count);
constexpr size_t SlowLogLimit = 24;
constexpr double SlowFrameMs = 1000.0 / 30.0;

std::array<std::atomic<u64>, CategoryCount> pending_ns{};

struct Session {
    bool enabled{};
    Clock::time_point previous{};
    Clock::time_point window_start{};
    std::vector<double> intervals;
    size_t frame{};
    size_t slow{};
    size_t over_50_ms{};
};

Session session;

double Milliseconds(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

void ReportWindow(const char* reason) {
    if (session.intervals.empty()) {
        return;
    }
    auto& intervals = session.intervals;
    std::sort(intervals.begin(), intervals.end());
    const auto percentile = [&](size_t percent) {
        return intervals[(intervals.size() * percent + 99) / 100 - 1];
    };
    LOG_INFO(Render_Vulkan,
             "Vulkan frame trace {}: frames={} p50_ms={:.2f} p95_ms={:.2f} p99_ms={:.2f} "
             "max_ms={:.2f} over_33ms={} over_50ms={} slow_details_omitted={}",
             reason, intervals.size(), percentile(50), percentile(95), percentile(99),
             intervals.back(), session.slow, session.over_50_ms,
             session.slow - std::min(session.slow, SlowLogLimit));
    intervals.clear();
    session.slow = 0;
    session.over_50_ms = 0;
}

} // namespace

std::atomic_bool recording{};
thread_local const FlipTiming* current_flip{};

void Start(u64 program_id) {
    const char* value = std::getenv("EDEN_NX2_STUTTER_TRACE");
    if (!value) {
        return;
    }
    const std::string_view selection{value};
    if (selection != "1" && selection != fmt::format("{:016X}", program_id)) {
        return;
    }
    for (auto& counter : pending_ns) {
        counter.store(0, std::memory_order_relaxed);
    }
    session.enabled = true;
    session.intervals.reserve(1024);
    LOG_INFO(Render_Vulkan,
             "Vulkan frame trace enabled for {:016X}: base frames, host wall times; "
             "operations can overlap and are counted in the interval where they complete",
             program_id);
}

void OnFrame() {
    if (!session.enabled) {
        return;
    }
    const auto now = Clock::now();
    if (session.previous == Clock::time_point{}) {
        session.previous = session.window_start = now;
        recording.store(true, std::memory_order_relaxed);
        return;
    }
    const double interval = Milliseconds(now - session.previous);
    session.previous = now;
    ++session.frame;
    session.intervals.push_back(interval);
    session.over_50_ms += interval > 50.0;

    std::array<double, CategoryCount> durations{};
    for (size_t i = 0; i < CategoryCount; ++i) {
        durations[i] = static_cast<double>(pending_ns[i].exchange(0, std::memory_order_relaxed)) /
                       1e6;
    }
    if (interval > SlowFrameMs && ++session.slow <= SlowLogLimit) {
        LOG_INFO(Render_Vulkan,
                 "Vulkan slow frame {}: frame_ms={:.2f} pipeline_request_ms={:.2f} "
                 "driver_build_ms={:.2f} draw_build_wait_ms={:.2f} gpu_wait_ms={:.2f} "
                 "worker_wait_ms={:.2f} readback_ms={:.2f} frame_acquire_ms={:.2f} present_ms={:.2f} "
                 "split_wait_ms={:.2f} readback_submit_ms={:.2f} astc_prepare_ms={:.2f} "
                 "dlss_prepare_ms={:.2f} dlss_evaluate_ms={:.2f} gpu_allocate_ms={:.2f}",
                 session.frame, interval, durations[0], durations[1], durations[2], durations[3],
                 durations[4], durations[5], durations[6], durations[7], durations[8], durations[9],
                 durations[10], durations[11], durations[12], durations[13]);
        if (const auto* flip = current_flip) {
            LOG_INFO(Render_Vulkan,
                     "Vulkan slow frame {} guest flip {}: vi_gap_ms={:.2f} vi_fence_ms={:.2f} "
                     "vi_prepare_ms={:.2f} gpu_queue_ms={:.2f} guest_syncpoint_ms={:.2f} "
                     "composite_entry_ms={:.2f} fence_id={} fence_target={} "
                     "fence_guest_at_dispatch={}",
                     session.frame, flip->id, Milliseconds(flip->guest_gap),
                     Milliseconds(flip->service_wait), Milliseconds(flip->queued - flip->service_ready),
                     Milliseconds(flip->dispatched - flip->queued),
                     Milliseconds(flip->compositing - flip->dispatched),
                     Milliseconds(now - flip->compositing), flip->fence_id, flip->fence_target,
                     flip->fence_guest_at_dispatch);

            const auto elapsed = [&](Category category) {
                return durations[static_cast<size_t>(category)];
            };
            LOG_INFO(Render_Vulkan,
                     "Vulkan slow frame {} guest GPU work: command_list_ms={:.2f} "
                     "pending_resume_ms={:.2f} pending_backoff_ms={:.2f} command_idle_ms={:.2f} "
                     "raster_graphics_ms={:.2f} raster_compute_ms={:.2f} texture_upload_ms={:.2f}",
                     session.frame, elapsed(Category::GuestCommandList),
                     elapsed(Category::GuestPendingResume), elapsed(Category::GuestPendingBackoff),
                     elapsed(Category::GuestCommandIdle), elapsed(Category::RasterGraphics),
                     elapsed(Category::RasterCompute), elapsed(Category::TextureUpload));
            if (interval >= 50.0) {
                LOG_INFO(Render_Vulkan,
                         "Vulkan slow frame {} graphics preparation: image_views_ms={:.2f} "
                         "buffers_ms={:.2f} render_targets_ms={:.2f} flush_ms={:.2f} "
                         "texture_unswizzle_ms={:.2f} texture_overlap_ms={:.2f}",
                         session.frame, elapsed(Category::GraphicsImageViews),
                         elapsed(Category::GraphicsBuffers), elapsed(Category::GraphicsRenderTargets),
                         elapsed(Category::GraphicsFlush), elapsed(Category::TextureUnswizzle),
                         elapsed(Category::TextureOverlap));
                LOG_INFO(Render_Vulkan,
                         "Vulkan slow frame {} texture preparation detail: driver_view_create_ms={:.2f} "
                         "track_ms={:.2f} lookup_ms={:.2f} aliases_ms={:.2f} "
                         "guest_read_ms={:.2f} staging_ms={:.2f} upload_record_ms={:.2f}",
                         session.frame, elapsed(Category::ImageViewCreate),
                         elapsed(Category::TextureTrack), elapsed(Category::TextureLookup),
                         elapsed(Category::TextureAliases), elapsed(Category::TextureRead),
                         elapsed(Category::TextureStaging), elapsed(Category::TextureRecord));
            }
        }
    }
    if (now - session.window_start >= 10s) {
        ReportWindow("window");
        session.window_start = now;
    }
}

void Stop() {
    if (!session.enabled) {
        return;
    }
    recording.store(false, std::memory_order_relaxed);
    ReportWindow("closing window");
    session = {};
}

void Record(Category category, Clock::duration duration) {
    if (recording.load(std::memory_order_relaxed)) [[unlikely]] {
        pending_ns[static_cast<size_t>(category)].fetch_add(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(),
            std::memory_order_relaxed);
    }
}

} // namespace VideoCommon::StutterTrace
