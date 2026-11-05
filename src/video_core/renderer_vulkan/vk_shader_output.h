// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include "shader_recompiler/runtime_info.h"
#include "video_core/surface.h"

namespace Vulkan {

inline std::array<Shader::AttributeType, 8> FragmentColorOutputTypes(
    const std::array<u8, 8>& color_formats) {
    std::array<Shader::AttributeType, 8> types{};
    types.fill(Shader::AttributeType::Float);
    for (size_t index = 0; index < color_formats.size(); ++index) {
        const auto format = static_cast<Tegra::RenderTargetFormat>(color_formats[index]);
        if (format == Tegra::RenderTargetFormat::NONE) {
            continue;
        }
        const auto pixel_format = VideoCore::Surface::PixelFormatFromRenderTargetFormat(format);
        if (VideoCore::Surface::IsPixelFormatInteger(pixel_format)) {
            types[index] = VideoCore::Surface::IsPixelFormatSignedInteger(pixel_format)
                               ? Shader::AttributeType::SignedInt
                               : Shader::AttributeType::UnsignedInt;
        }
    }
    return types;
}

} // namespace Vulkan
