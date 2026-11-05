// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <optional>
#include <span>

#include "shader_recompiler/shader_info.h"
#include "video_core/texture_cache/types.h"

namespace Vulkan {

inline bool IsReadOnlyBufferTextureArray(
    const Shader::TextureDescriptor& desc,
    std::span<const Shader::StorageBufferDescriptor> buffers) {
    const auto& source = desc.buffer_source;
    return source.enabled && desc.count > 1 && source.uniform_index.empty() &&
           !source.width_query && desc.type == Shader::TextureType::Color2D &&
           !desc.is_integer && !desc.is_multisample && !desc.has_secondary &&
           std::ranges::any_of(buffers, [&](const auto& buffer) {
               return buffer.cbuf_index == desc.cbuf_index &&
                      buffer.cbuf_offset == desc.cbuf_offset && buffer.count == 1 &&
                      !buffer.is_written;
           });
}

inline std::optional<Shader::TextureType> ActualBufferTextureView(
    VideoCommon::ImageViewType type, bool is_integer) {
    if (type == VideoCommon::ImageViewType::e3D && !is_integer) {
        return Shader::TextureType::Color3D;
    }
    if (type == VideoCommon::ImageViewType::e2D && is_integer) {
        return Shader::TextureType::Color2D;
    }
    return std::nullopt;
}

// Private comparison scope reproducing the original captured-array diagnostic.
inline bool MatchesLegacyBufferTextureArrayScope(
    const Shader::TextureDescriptor& desc,
    std::span<const Shader::StorageBufferDescriptor> buffers) {
    const auto& source = desc.buffer_source;
    return desc.cbuf_index == 0 && desc.cbuf_offset == 0x520 && source.stride == 96 &&
           source.offset == 0 && std::ranges::any_of(buffers, [](const auto& buffer) {
               return buffer.cbuf_index == 0 && buffer.cbuf_offset == 0x510 &&
                      buffer.count == 1 && !buffer.is_written;
           });
}

} // namespace Vulkan
