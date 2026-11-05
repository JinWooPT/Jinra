// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <span>
#include <vector>

#include "audio_core/audio_core.h"
#include "audio_core/common/common.h"
#include "audio_core/sink/sink_stream.h"
#include "common/common_types.h"
#include "common/fixed_point.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"

namespace AudioCore::Sink {

SinkStream::SinkStream(Core::System& system_, StreamType type_)
    : system{system_}, type{type_}, underrun_stats_enabled{[] {
          const char* value = std::getenv("EDEN_AUDIO_UNDERRUN_STATS");
          return value && value[0] != '0' && std::atomic<u64>::is_always_lock_free;
      }()} {
    if (underrun_stats_enabled) {
        last_underrun_report = std::chrono::steady_clock::now();
    }
}

void SinkStream::AppendBuffer(SinkBuffer& buffer, std::span<s16> samples) {
    if (type == StreamType::In)
        return;

    constexpr s32 min = (std::numeric_limits<s16>::min)();
    constexpr s32 max = (std::numeric_limits<s16>::max)();

    auto yuzu_volume = Settings::Volume();
    if (yuzu_volume > 1.0f)
        yuzu_volume = 0.6f + 20.0f * std::log10(yuzu_volume);
    auto const volume = system_volume * device_volume * yuzu_volume;

    if (system_channels > device_channels) {
        static constexpr std::array<f32, 4> tcoeff{1.0f, 0.596f, 0.354f, 0.707f};
        for (u32 r_offs = 0, w_offs = 0; r_offs < samples.size(); r_offs += system_channels, w_offs += device_channels) {
            std::array<f32, 6> ccoeff{0.f};
            for (u32 i = 0; i < system_channels; ++i)
                ccoeff[i] = f32(samples[r_offs + i]);

            std::array<f32, 6> rcoeff{
                ccoeff[u32(Channels::FrontLeft)],
                ccoeff[u32(Channels::BackLeft)],
                ccoeff[u32(Channels::Center)],
                ccoeff[u32(Channels::LFE)],
                ccoeff[u32(Channels::BackRight)],
                ccoeff[u32(Channels::FrontRight)],
            };

            const f32 left  = rcoeff[0] * tcoeff[0] + rcoeff[2] * tcoeff[1] + rcoeff[3] * tcoeff[2] + rcoeff[1] * tcoeff[3];
            const f32 right = rcoeff[5] * tcoeff[0] + rcoeff[2] * tcoeff[1] + rcoeff[3] * tcoeff[2] + rcoeff[4] * tcoeff[3];

            samples[w_offs + 0] = s16(std::clamp(s32(left * volume),  min, max));
            samples[w_offs + 1] = s16(std::clamp(s32(right * volume), min, max));
        }

        queue.EmplaceWait(buffer);
        samples_buffer.Push(samples.subspan(0, samples.size() / system_channels * device_channels));
    } else if (system_channels < device_channels) {
        std::vector<s16> new_samples(samples.size() / system_channels * device_channels);
        for (u32 r_offs = 0, w_offs = 0; r_offs < samples.size(); r_offs += system_channels, w_offs += device_channels)
            for (u32 channel = 0; channel < system_channels; ++channel)
                new_samples[w_offs + channel] = s16(std::clamp(s32(f32(samples[r_offs + channel]) * volume), min, max));

        queue.EmplaceWait(buffer);
        samples_buffer.Push(new_samples);
    } else {
        if (volume != 1.0f) {
            for (u32 i = 0; i < samples.size(); ++i)
                samples[i] = s16(std::clamp(s32(f32(samples[i]) * volume), min, max));
        }

        queue.EmplaceWait(buffer);
        samples_buffer.Push(samples);
    }

    ++queued_buffers;
    if (underrun_stats_enabled) {
        ReportUnderruns();
    }
}

void SinkStream::ReportUnderruns() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = now - last_underrun_report;
    if (elapsed < std::chrono::seconds{2}) return;
    last_underrun_report = now;
    const auto callbacks = underrun_stats.callbacks.exchange(0, std::memory_order_relaxed);
    const auto requested = underrun_stats.requested_frames.exchange(0, std::memory_order_relaxed);
    const auto underruns = underrun_stats.underrun_callbacks.exchange(0, std::memory_order_relaxed);
    const auto missing = underrun_stats.missing_frames.exchange(0, std::memory_order_relaxed);
    const auto gap = underrun_stats.max_queue_gap_frames.exchange(0, std::memory_order_relaxed);
    const auto short_callbacks = underrun_stats.short_sample_callbacks.exchange(0, std::memory_order_relaxed);
    const auto short_frames = underrun_stats.short_sample_frames.exchange(0, std::memory_order_relaxed);
    constexpr double frame_ms = 1000.0 / TargetSampleRate;
    LOG_INFO(Audio,
             "Audio sink underruns: stream={} type={} interval_ms={:.1f} callbacks={} "
             "requested_frames={} underrun_callbacks={} missing_frames={} missing_ms={:.2f} "
             "max_queue_gap_ms={:.2f} short_sample_callbacks={} short_sample_frames={}",
             static_cast<const void*>(this), type == StreamType::Render ? "render" : "out",
             std::chrono::duration<double, std::milli>(elapsed).count(), callbacks, requested,
             underruns, missing, missing * frame_ms, gap * frame_ms, short_callbacks, short_frames);
}

std::vector<s16> SinkStream::ReleaseBuffer(u64 num_samples) {
    auto samples{samples_buffer.Pop(num_samples)};

    // TODO: Up-mix to 6 channels if the game expects it.
    // For audio input this is unlikely to ever be the case though.

    // Incoming mic volume seems to always be very quiet, so multiply by an additional 8 here.
    // TODO: Play with this and find something that works better.
    constexpr s32 min = (std::numeric_limits<s16>::min)();
    constexpr s32 max = (std::numeric_limits<s16>::max)();
    auto volume{system_volume * device_volume * 8};
    for (u32 i = 0; i < samples.size(); i++)
        samples[i] = s16(std::clamp(s32(f32(samples[i]) * volume), min, max));

    if (samples.size() < num_samples)
        samples.resize(num_samples, 0);
    return samples;
}

void SinkStream::ClearQueue() {
    std::scoped_lock lk{release_mutex};

    samples_buffer.Pop();
    SinkBuffer tmp;
    while (queue.TryPop(tmp));

    queued_buffers = 0;
    playing_buffer = {};
    playing_buffer.consumed = true;
}

void SinkStream::ProcessAudioIn(std::span<const s16> input_buffer, std::size_t num_frames) {
    const std::size_t num_channels = GetDeviceChannels();
    const std::size_t frame_size = num_channels;
    const std::size_t frame_size_bytes = frame_size * sizeof(s16);
    size_t frames_written{0};

    // If we're paused or going to shut down, we don't want to consume buffers as coretiming is
    // paused and we'll desync, so just return.
    if (system.IsPaused() || system.IsShuttingDown())
        return;

    while (frames_written < num_frames) {
        // If the playing buffer has been consumed or has no frames, we need a new one
        if (playing_buffer.consumed || playing_buffer.frames == 0) {
            if (!queue.TryPop(playing_buffer)) {
                // If no buffer was available we've underrun, just push the samples and
                // continue.
                samples_buffer.Push(&input_buffer[frames_written * frame_size], (num_frames - frames_written) * frame_size);
                frames_written = num_frames;
                continue;
            }
            // Successfully dequeued a new buffer.
            queued_buffers--;
        }

        // Get the minimum frames available between the currently playing buffer, and the
        // amount we have left to fill
        size_t frames_available{std::min<u64>(playing_buffer.frames - playing_buffer.frames_played, num_frames - frames_written)};

        samples_buffer.Push(&input_buffer[frames_written * frame_size], frames_available * frame_size);

        frames_written += frames_available;
        playing_buffer.frames_played += frames_available;

        // If that's all the frames in the current buffer, add its samples and mark it as
        // consumed
        if (playing_buffer.frames_played >= playing_buffer.frames)
            playing_buffer.consumed = true;
    }

    if (frames_written > 0) {
        std::memcpy(&last_frame[0], &input_buffer[(frames_written - 1) * frame_size], frame_size_bytes);
    }

    // update sample counts fuer audio-ins
    {
        std::scoped_lock lk{sample_count_lock};
        last_sample_count_update_time = system.CoreTiming().GetGlobalTimeNs();
        min_played_sample_count = max_played_sample_count;
        max_played_sample_count += frames_written;
    }
}

void SinkStream::ProcessAudioOutAndRender(std::span<s16> output_buffer, std::size_t num_frames) {
    const std::size_t num_channels = GetDeviceChannels();
    const std::size_t frame_size = num_channels;
    const std::size_t frame_size_bytes = frame_size * sizeof(s16);
    size_t frames_written{0};
    size_t actual_frames_written{0};
    size_t short_sample_frames{0};

    if (system.IsPaused() || system.IsShuttingDown()) {
        if (underrun_stats_enabled) queue_gap_frames = 0;
        if (system.IsShuttingDown()) {
            std::scoped_lock lk{release_mutex};
            queued_buffers.store(0);
            release_cv.notify_one();
        }
        for (size_t i = 0; i < num_frames; i++)
            std::memset(&output_buffer[i * frame_size], 0, frame_size_bytes);
        return;
    }

    while (frames_written < num_frames) {
        if (playing_buffer.consumed || playing_buffer.frames == 0) {
            std::unique_lock lk{release_mutex};

            if (!queue.TryPop(playing_buffer)) {
                lk.unlock();
                for (size_t i = frames_written; i < num_frames; i++)
                    std::memcpy(&output_buffer[i * frame_size], last_frame.data(), frame_size_bytes);
                frames_written = num_frames;
                continue;
            }

            --queued_buffers;
            lk.unlock();
            release_cv.notify_one();
        }

        const size_t frames_available = std::min<u64>(playing_buffer.frames - playing_buffer.frames_played, num_frames - frames_written);

        const auto samples_read =
            samples_buffer.Pop(&output_buffer[frames_written * frame_size], frames_available * frame_size);
        // AppendBuffer publishes queue metadata before its samples. Count a short sample read
        // separately from an empty queue so those two conditions can be distinguished.
        if (underrun_stats_enabled && samples_read < frames_available * frame_size) {
            short_sample_frames +=
                (frames_available * frame_size - samples_read + frame_size - 1) / frame_size;
        }

        frames_written += frames_available;
        actual_frames_written += frames_available;
        playing_buffer.frames_played += frames_available;

        if (playing_buffer.frames_played >= playing_buffer.frames)
            playing_buffer.consumed = true;
    }

    if (underrun_stats_enabled) {
        underrun_stats.callbacks.fetch_add(1, std::memory_order_relaxed);
        underrun_stats.requested_frames.fetch_add(num_frames, std::memory_order_relaxed);
        const u64 missing_frames = num_frames - actual_frames_written;
        if (actual_frames_written || !missing_frames) queue_gap_frames = 0;
        if (missing_frames) {
            queue_gap_frames += missing_frames;
            underrun_stats.underrun_callbacks.fetch_add(1, std::memory_order_relaxed);
            underrun_stats.missing_frames.fetch_add(missing_frames, std::memory_order_relaxed);
            auto previous = underrun_stats.max_queue_gap_frames.load(std::memory_order_relaxed);
            while (previous < queue_gap_frames &&
                   !underrun_stats.max_queue_gap_frames.compare_exchange_weak(
                       previous, queue_gap_frames, std::memory_order_relaxed)) {}
        }
        if (short_sample_frames) {
            underrun_stats.short_sample_callbacks.fetch_add(1, std::memory_order_relaxed);
            underrun_stats.short_sample_frames.fetch_add(short_sample_frames, std::memory_order_relaxed);
        }
    }

    std::memcpy(last_frame.data(), &output_buffer[(frames_written - 1) * frame_size], frame_size_bytes);

    // EDEN_NX2_AUDIO_LEVEL: log the output peak about every two seconds (is a game audible?).
    if (static const bool meter = std::getenv("EDEN_NX2_AUDIO_LEVEL") != nullptr; meter) {
        for (size_t i = 0; i < num_frames * frame_size; ++i) {
            level_peak = std::max<s32>(level_peak, std::abs(static_cast<s32>(output_buffer[i])));
        }
        level_frames += num_frames;
        if (level_frames >= TargetSampleRate * 2) {
            LOG_INFO(Audio, "Audio output peak over {} frames: {} of 32767", level_frames, level_peak);
            level_frames = 0;
            level_peak = 0;
        }
    }

    {
        std::scoped_lock lk{sample_count_lock};
        last_sample_count_update_time = system.CoreTiming().GetGlobalTimeNs();
        min_played_sample_count = max_played_sample_count;
        max_played_sample_count += actual_frames_written;
    }
}

u64 SinkStream::GetExpectedPlayedSampleCount() {
    std::scoped_lock lk{sample_count_lock};
    auto cur_time{system.CoreTiming().GetGlobalTimeNs()};
    auto time_delta{cur_time - last_sample_count_update_time};
    auto exp_played_sample_count{min_played_sample_count + (TargetSampleRate * time_delta) / std::chrono::seconds{1}};

    // Add 25ms of latency in sample reporting to allow for some leeway in scheduler timings
    return std::min<u64>(exp_played_sample_count, max_played_sample_count) + TargetSampleCount * 5;
}

void SinkStream::WaitFreeSpace(std::stop_token stop_token) {
    std::unique_lock lk{release_mutex};

    auto can_continue = [this]() {
        return paused || queued_buffers < max_queue_size;
    };

    release_cv.wait_for(lk, std::chrono::milliseconds(7), can_continue);

    if (queued_buffers > max_queue_size + 3) {
        release_cv.wait(lk, stop_token, can_continue);
    }
}

void SinkStream::SignalPause() {
    {
        std::scoped_lock lk{release_mutex};
        paused = true;
    }
    release_cv.notify_one();
}

} // namespace AudioCore::Sink
