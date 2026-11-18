// SPDX-FileCopyrightText: Copyright 2026 Fritchii
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/texture_cache/relocation_memory.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include "common/fs/path_util.h"
#include "common/logging.h"

namespace VideoCommon::RelocationMemory {
namespace {

/// TextureCache's GuardSampled (GuardLoaded = 2 and GuardCpuRead = 4 in the seed and the file).
constexpr u8 Sampled = 1;

/// TextureCacheBase::RelocationShapeKey of a single-level, single-layer, single-sample 2D image.
constexpr u64 Key(u32 format, u32 width, u32 height) {
    return (u64{format & 0xFF} << 56) | (u64{1} << 52) | (u64{1} << 48) | (u64{1} << 36) |
           (u64{width & 0x3FFFF} << 18) | (height & 0x3FFFF);
}

struct Seed {
    u32 format;
    u32 width;
    u32 height;
    u8 guard;
};

/// Shapes guarded in 30 or more of our sessions' logs, or seen losing their contents to a
/// relocation (2026-11-18, 602 logs). Format numbers are VideoCore::Surface::PixelFormat.
constexpr Seed Seeds[] = {
    {0, 64, 62, 1},       {0, 96, 77, 3},       {0, 128, 34, 1},      {0, 128, 128, 2},
    {0, 160, 40, 2},      {0, 176, 40, 2},      {0, 224, 40, 3},      {0, 240, 94, 1},
    {0, 320, 33, 1},      {0, 592, 49, 1},      {0, 640, 360, 1},     {0, 848, 133, 1},
    {0, 960, 540, 1},     {0, 1280, 720, 1},    {0, 1920, 1080, 1},   {0, 2560, 1440, 3},
    {7, 1280, 720, 3},    {7, 1920, 1080, 1},   {12, 128, 77, 2},     {12, 128, 94, 2},
    {12, 128, 96, 2},     {12, 128, 126, 3},    {12, 128, 128, 3},    {12, 192, 97, 2},
    {12, 192, 98, 3},     {12, 192, 110, 2},    {12, 192, 112, 2},    {12, 192, 192, 3},
    {12, 256, 105, 2},    {12, 320, 98, 2},     {12, 320, 104, 2},    {12, 384, 97, 3},
    {12, 512, 270, 2},    {12, 1280, 720, 3},   {12, 2560, 1440, 1},  {15, 192, 90, 2},
    {16, 256, 256, 3},    {16, 512, 512, 3},    {16, 960, 540, 1},    {16, 1280, 720, 1},
    {21, 16, 1, 2},       {21, 16, 2, 2},       {21, 16, 4, 2},       {21, 16, 8, 2},
    {21, 32, 16, 2},      {21, 64, 33, 2},      {21, 128, 67, 2},     {21, 240, 135, 3},
    {21, 480, 270, 3},    {21, 640, 360, 3},    {21, 960, 540, 1},    {21, 1280, 720, 3},
    {21, 1920, 1080, 1},  {21, 2272, 172, 2},   {21, 2560, 1440, 3},  {35, 4, 1, 2},
    {39, 640, 360, 3},    {39, 1280, 720, 3},   {39, 2560, 1440, 3},  {40, 256, 256, 1},
    {40, 1280, 720, 3},   {46, 256, 256, 3},    {46, 512, 512, 1},    {47, 640, 360, 1},
    {51, 1920, 1080, 4},  {51, 2560, 1440, 6},  {105, 640, 360, 3},   {105, 1280, 720, 3},
    {105, 2560, 1440, 3}, {105, 4096, 4096, 1}, {106, 4096, 4096, 3}, {106, 12288, 12288, 3},
};

/// D32_FLOAT: OoT's square depth targets, every size from 64 to 2560 in steps of 64 (each size
/// seen is guarded "sampled in a later frame" within seconds of its first use).
constexpr u32 SquareDepthFormat = 105;

std::mutex g_mutex;
std::unordered_map<u64, u8> g_known; ///< What the file holds (plus the seed)
bool g_loaded{};

std::filesystem::path FilePath() {
    return Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "relocation_guards.txt";
}

} // Anonymous namespace

bool Enabled() {
    static const bool on = [] {
        const char* const value = std::getenv("FRITCHII_RELOCATION_MEMORY");
        return value == nullptr || value[0] == '\0' || std::strtol(value, nullptr, 10) != 0;
    }();
    return on;
}

std::vector<std::pair<u64, u8>> Load() {
    std::scoped_lock lock{g_mutex};
    if (!g_loaded) {
        g_loaded = true;
        for (const Seed& seed : Seeds) {
            g_known[Key(seed.format, seed.width, seed.height)] |= seed.guard;
        }
        for (u32 size = 64; size <= 2560; size += 64) {
            g_known[Key(SquareDepthFormat, size, size)] |= Sampled;
        }
        const size_t seeded = g_known.size();
        size_t learned = 0;
        std::ifstream file{FilePath()};
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream fields{line};
            std::string key_text;
            unsigned guard = 0;
            if (!(fields >> key_text >> guard) || guard == 0 || guard > 7) {
                continue;
            }
            const u64 key = std::strtoull(key_text.c_str(), nullptr, 16);
            if (key != 0) {
                g_known[key] |= static_cast<u8>(guard);
                ++learned;
            }
        }
        LOG_INFO(HW_GPU,
                 "Render target relocation memory (FRITCHII_RELOCATION_MEMORY=1): {} shapes "
                 "guarded from the start ({} seeded, {} lines learned in earlier sessions)",
                 g_known.size(), seeded, learned);
    }
    return {g_known.begin(), g_known.end()};
}

void Note(u64 shape_key, u8 guard) {
    std::scoped_lock lock{g_mutex};
    u8& known = g_known[shape_key];
    if ((known | guard) == known) {
        return;
    }
    known |= guard;
    std::error_code error;
    std::filesystem::create_directories(FilePath().parent_path(), error);
    std::ofstream file{FilePath(), std::ios::app};
    file << fmt::format("{:016x} {}\n", shape_key, static_cast<unsigned>(known));
}

} // namespace VideoCommon::RelocationMemory
