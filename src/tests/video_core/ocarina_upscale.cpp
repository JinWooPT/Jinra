// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>
#include "video_core/ocarina_upscale.h"
#include "video_core/fsr.h"

TEST_CASE("OoT temporal backend selection is opt-in and vendor neutral", "[video_core][ocarina]") {
    using enum VideoCommon::OcarinaUpscaleBackend;
    using VideoCommon::SelectOcarinaUpscaler;
    CHECK(SelectOcarinaUpscaler(false, false, false) == Native);
    CHECK(SelectOcarinaUpscaler(false, false, true) == Native);
    CHECK(SelectOcarinaUpscaler(true, false, false) == Fsr);
    CHECK(SelectOcarinaUpscaler(true, false, true) == Fsr);
    CHECK(SelectOcarinaUpscaler(false, true, false) == Native);
    CHECK(SelectOcarinaUpscaler(false, true, true) == Dlss);
    CHECK(SelectOcarinaUpscaler(true, true, false) == Fsr);
    CHECK(SelectOcarinaUpscaler(true, true, true) == Dlss);
}

TEST_CASE("FSR temporal sharpness uses the existing stops slider", "[video_core][fsr][ocarina]") {
    CHECK(FSR::RcasSharpnessFromStops(0.0f) == 1.0f);
    CHECK(FSR::RcasSharpnessFromStops(0.81f) == 0.595f);
    CHECK(FSR::RcasSharpnessFromStops(1.0f) == 0.5f);
    CHECK(FSR::RcasSharpnessFromStops(2.0f) == 0.0f);
    CHECK(FSR::RcasSharpnessFromStops(3.0f) == 0.0f);
    CHECK(FSR::RcasSharpnessFromStops(-1.0f) == 1.0f);
}

TEST_CASE("Ocarina texture bias applies only to docked FSR texture inputs", "[video_core][ocarina]") {
    using enum VideoCommon::OcarinaUpscaleBackend;
    CHECK(VideoCommon::OcarinaFsrLodBias(Native) == 0.0f);
    CHECK(VideoCommon::OcarinaFsrLodBias(Dlss) == 0.0f);
    CHECK(VideoCommon::OcarinaFsrLodBias(Fsr) == -1.58f);
    CHECK(VideoCommon::KeepOcarinaFsrLodBias(true, false, false));
    CHECK_FALSE(VideoCommon::KeepOcarinaFsrLodBias(false, false, false));
    CHECK_FALSE(VideoCommon::KeepOcarinaFsrLodBias(true, true, false));
    CHECK_FALSE(VideoCommon::KeepOcarinaFsrLodBias(true, false, true));
}
