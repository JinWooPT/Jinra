// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include "common/common_types.h"

namespace VideoCommon::StutterTrace {

enum class Category {
    PipelineRequest,
    DriverBuild,
    DrawBuildWait,
    GpuWait,
    WorkerWait,
    Readback,
    FrameAcquire,
    Present,
    SplitWait,
    ReadbackSubmit,
    AstcPrepare,
    DlssPrepare,
    DlssEvaluate,
    GpuAllocate,
    GuestCommandList,
    GuestPendingResume,
    GuestPendingBackoff,
    GuestCommandIdle,
    RasterGraphics,
    RasterCompute,
    TextureUpload,
    Count,
};

// The renderer starts and ends the session. Workers contribute completed wall-clock durations.
extern std::atomic_bool recording;

struct FlipTiming {
    using Clock = std::chrono::steady_clock;

    u64 id{};
    Clock::duration guest_gap{};
    Clock::duration service_wait{};
    Clock::time_point service_ready{};
    Clock::time_point queued{};
    Clock::time_point dispatched{};
    Clock::time_point compositing{};
    s32 fence_id{-1};
    u32 fence_target{};
    u32 fence_guest_at_dispatch{};
};

// The producer and renderer scopes are thread-local. The queue copies the timing with the flip.
extern thread_local const FlipTiming* current_flip;

class FlipScope {
public:
    explicit FlipScope(const FlipTiming& flip) : previous{current_flip} {
        current_flip = flip.id != 0 ? &flip : nullptr;
    }
    ~FlipScope() { current_flip = previous; }

    FlipScope(const FlipScope&) = delete;
    FlipScope& operator=(const FlipScope&) = delete;

private:
    const FlipTiming* previous;
};

void Start(u64 program_id);
void OnFrame();
void Stop();
void Record(Category category, std::chrono::steady_clock::duration duration);

class Scope {
public:
    explicit Scope(Category category_) : category{category_} {
        if (recording.load(std::memory_order_relaxed)) [[unlikely]] {
            start = std::chrono::steady_clock::now();
        }
    }

    ~Scope() {
        if (start != std::chrono::steady_clock::time_point{}) [[unlikely]] {
            Record(category, std::chrono::steady_clock::now() - start);
        }
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Category category;
    std::chrono::steady_clock::time_point start{};
};

} // namespace VideoCommon::StutterTrace
