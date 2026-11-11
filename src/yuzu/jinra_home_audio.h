// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>

struct SDL_AudioStream;

class JinraHomeAudio {
public:
    enum class Cue { Navigate, Confirm, Back, Notice, Startup };

    JinraHomeAudio();
    ~JinraHomeAudio();
    JinraHomeAudio(const JinraHomeAudio&) = delete;
    JinraHomeAudio& operator=(const JinraHomeAudio&) = delete;

    void Play(Cue cue);
    int Volume() const { return volume; }
    void SetVolume(int percent, bool save = true);

private:
    SDL_AudioStream* stream{};
    QElapsedTimer navigation_clock;
    int volume{};
    bool audio_initialized{};
    bool audio_attempted{};
};
