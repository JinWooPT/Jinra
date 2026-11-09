// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/nx2_precompile.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>

#include <fmt/format.h>

#include "common/cityhash.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "common/scope_exit.h"
#include "core/file_sys/nx2_shader_extract.h"
#include "video_core/nx2_recipe.h"

namespace VideoCommon::Nx2Precompile {
namespace {

std::mutex game_mutex;
u64 game_title{};
std::optional<u32> game_version;
OpenGameRomFs game_romfs;

} // namespace

void SetGame(u64 title, std::optional<u32> version, OpenGameRomFs open_romfs) {
    std::scoped_lock lock{game_mutex};
    game_title = title;
    game_version = version;
    game_romfs = std::move(open_romfs);
}

std::filesystem::path PrepareFile(u64 title, const std::filesystem::path& directory,
                                  u32 cache_version, std::stop_token stop) try {
    std::optional<u32> version;
    {
        std::scoped_lock lock{game_mutex};
        if (title != game_title || !game_romfs || !game_version) {
            return {};
        }
        version = game_version;
    }
    const auto path =
        directory / fmt::format("precompiled-{}.fork-v{}.nx2prep", *version, cache_version);
    auto note = path;
    note += ".recipe";
    u64 previous_digest{};
    std::error_code saved_error;
    const bool has_saved_preparation = (std::ifstream(note) >> previous_digest) &&
                                       std::filesystem::is_regular_file(path, saved_error);

    constexpr u64 MaxRecipeBytes = u64{16} << 20;
    std::vector<std::filesystem::path> candidates;
    for (const auto& folder : {directory, Common::FS::GetExeDirectory() / "recipes"}) {
        std::error_code error;
        for (std::filesystem::directory_iterator it{folder, error}, end; !error && it != end;
             it.increment(error)) {
            if (it->path().extension() == ".nx2recipe") {
                candidates.push_back(it->path());
            }
        }
    }
    std::ranges::sort(candidates);
    bool found_matching_recipe = false;
    for (const auto& candidate : candidates) {
        if (stop.stop_requested()) {
            return {};
        }
        std::error_code error;
        const auto size = std::filesystem::file_size(candidate, error);
        if (error || size == 0 || size > MaxRecipeBytes) {
            continue;
        }
        std::ifstream input(candidate, std::ios::binary);
        std::vector<u8> bytes(size);
        if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
            continue;
        }
        const auto header = Nx2Recipe::ReadHeader(bytes);
        if (!header || header->title_id != title || header->cache_version != cache_version ||
            header->game_version != *version) {
            continue;
        }

        found_matching_recipe = true;
        const auto digest = Common::CityHash64(reinterpret_cast<const char*>(bytes.data()), size);
        if (has_saved_preparation && previous_digest == digest) {
            return path;
        }

        FileSys::VirtualDir romfs;
        {
            std::scoped_lock lock{game_mutex};
            if (title != game_title || version != game_version || !game_romfs) {
                return {};
            }
            romfs = game_romfs();
        }
        if (!romfs) {
            return {};
        }
        LOG_INFO(Render_Vulkan, "Reading this game's shaders for preparation (version {})",
                 *version);
        const auto threads = std::clamp(std::thread::hardware_concurrency() / 4, 1u, 6u);
        const auto programs = FileSys::Nx2ShaderExtract::Extract(std::array{romfs}, threads,
                                                                nullptr, stop);
        if (stop.stop_requested()) {
            return {};
        }
        Nx2Recipe::Report report;
        const auto prepared = Nx2Recipe::Assemble(bytes, programs, cache_version, report, threads,
                                                   stop);
        if (stop.stop_requested() || report.records == 0) {
            return {};
        }
        auto temporary = path;
        temporary += fmt::format(".{}.tmp",
            std::chrono::steady_clock::now().time_since_epoch().count());
        SCOPE_EXIT { std::filesystem::remove(temporary, error); };
        std::ofstream output(temporary, std::ios::binary);
        output.write(reinterpret_cast<const char*>(prepared.data()), prepared.size());
        output.close();
        if (!output) {
            LOG_WARNING(Render_Vulkan, "Could not write the assembled shader preparation");
            return {};
        }
        std::filesystem::rename(temporary, path, error);
        if (error) {
            LOG_WARNING(Render_Vulkan, "Could not move the assembled shader preparation: {}",
                        error.message());
            return {};
        }
        std::ofstream marker(note, std::ios::trunc);
        marker << digest << '\n';
        LOG_INFO(Render_Vulkan, "Assembled {} prepared pipeline records", report.records);
        return path;
    }
    if (!found_matching_recipe && has_saved_preparation && !stop.stop_requested()) {
        return path;
    }
    return {};
} catch (const std::exception& error) {
    LOG_WARNING(Render_Vulkan, "Could not prepare game shader recipe: {}", error.what());
    return {};
}

} // namespace VideoCommon::Nx2Precompile
