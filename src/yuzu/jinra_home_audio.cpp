// SPDX-License-Identifier: GPL-3.0-or-later
#include "yuzu/jinra_home_audio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
#include <QSettings>
#include <SDL3/SDL.h>

#include "common/fs/path_util.h"

namespace {
constexpr int SampleRate = 48000;
constexpr int Channels = 2;
constexpr double Tau = 2 * std::numbers::pi;

QString HomeConfigPath() {
    const auto path = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "home.ini";
    return QString::fromStdString(Common::FS::PathToUTF8String(path));
}

double StartupSample(double time) {
    constexpr std::array<double, 4> notes{220, 330, 440, 659.255};
    double value = 0;
    for (std::size_t index = 0; index < notes.size(); ++index) {
        const double note_time = time - index * 0.13;
        if (note_time >= 0) {
            value += std::sin(Tau * notes[index] * note_time) *
                     std::min(1.0, note_time / 0.035) * std::exp(-2.8 * note_time) * 0.1;
        }
    }
    return value + std::sin(Tau * 55 * time) * std::min(1.0, time / 0.045) *
                       std::exp(-5 * time) * 0.14;
}

std::vector<std::int16_t> Synthesize(JinraHomeAudio::Cue cue) {
    using Cue = JinraHomeAudio::Cue;
    // Match the five synthesized HOME cues in the NoxieNX reference build.
    constexpr std::array<double, 5> durations{0.055, 0.17, 0.13, 0.28, 1.55};
    constexpr std::array<double, 4> pitches{720, 880, 440, 220};
    const auto kind = static_cast<std::size_t>(cue);
    const double duration = durations[kind];
    const int frames = static_cast<int>(duration * SampleRate);
    std::vector<std::int16_t> samples(frames * Channels);
    for (int frame = 0; frame < frames; ++frame) {
        const double time = static_cast<double>(frame) / SampleRate;
        double value;
        if (cue == Cue::Startup) {
            value = StartupSample(time);
        } else {
            const double pitch = pitches[kind] - (cue == Cue::Back ? 100 * time : 0);
            const double phase = Tau * pitch * time;
            value = (std::sin(phase) + 0.2 * std::sin(1.5 * phase)) * 0.2;
            value *= std::min(1.0, time / 0.006) * std::pow(1 - time / duration, 2);
        }
        value *= std::clamp((duration - time) / 0.035, 0.0, 1.0);
        const auto sample = static_cast<std::int16_t>(32767 * std::clamp(value, -0.8, 0.8));
        samples[frame * Channels] = sample;
        samples[frame * Channels + 1] = sample;
    }
    return samples;
}
} // namespace

JinraHomeAudio::JinraHomeAudio()
    : volume(std::clamp(QSettings(HomeConfigPath(), QSettings::IniFormat)
                            .value(QStringLiteral("volume"), 35).toInt(), 0, 100)) {}

JinraHomeAudio::~JinraHomeAudio() {
    if (stream) {
        SDL_DestroyAudioStream(stream);
    }
    if (audio_initialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void JinraHomeAudio::SetVolume(int percent, bool save) {
    volume = std::clamp(percent, 0, 100);
    if (save) {
        QSettings(HomeConfigPath(), QSettings::IniFormat).setValue(QStringLiteral("volume"), volume);
    }
    if (stream) {
        SDL_SetAudioStreamGain(stream, volume / 100.0f);
        if (!volume) {
            SDL_ClearAudioStream(stream);
        }
    }
}

void JinraHomeAudio::Play(Cue cue) {
    if (!volume) {
        return;
    }
    if (cue == Cue::Navigate) {
        if (navigation_clock.isValid() && navigation_clock.elapsed() < 85) {
            return;
        }
        navigation_clock.start();
    }
    if (!audio_attempted) {
        audio_attempted = true;
        audio_initialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
        if (audio_initialized) {
            const SDL_AudioSpec spec{SDL_AUDIO_S16, Channels, SampleRate};
            stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                                nullptr, nullptr);
            if (stream) {
                SDL_ResumeAudioStreamDevice(stream);
            }
        }
    }
    if (!stream) {
        return;
    }
    static const std::array samples{Synthesize(Cue::Navigate), Synthesize(Cue::Confirm),
                                    Synthesize(Cue::Back), Synthesize(Cue::Notice),
                                    Synthesize(Cue::Startup)};
    const auto& cue_samples = samples[static_cast<std::size_t>(cue)];
    SDL_ClearAudioStream(stream);
    SDL_SetAudioStreamGain(stream, volume / 100.0f);
    SDL_PutAudioStreamData(stream, cue_samples.data(),
                           static_cast<int>(cue_samples.size() * sizeof(cue_samples[0])));
    SDL_FlushAudioStream(stream);
}
