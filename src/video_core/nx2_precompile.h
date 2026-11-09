// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>

#include "common/common_types.h"
#include "core/file_sys/vfs/vfs.h"

namespace VideoCommon::Nx2Precompile {

using OpenGameRomFs = std::function<FileSys::VirtualDir()>;

void SetGame(u64 title, std::optional<u32> version, OpenGameRomFs open_romfs);

std::filesystem::path PrepareFile(u64 title, const std::filesystem::path& directory,
                                  u32 cache_version, std::stop_token stop);

} // namespace VideoCommon::Nx2Precompile
