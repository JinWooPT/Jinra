// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Vulkan {

struct DepthBiasParameters {
    float constant;
    float clamp;
    float slope;
};

// The fallback viewport path does not enlarge viewport dimensions.
constexpr float DepthBiasViewportScale(bool enabled, bool target_rescaled,
                                       bool viewport_scale_offset_enabled, float up_factor) {
    return enabled && target_rescaled && viewport_scale_offset_enabled ? up_factor : 1.0f;
}

constexpr DepthBiasParameters RescaleDepthBiasParameters(DepthBiasParameters parameters,
                                                       float viewport_scale) {
    // Framebuffer dz/dx and dz/dy shrink by the viewport scale. Constant depth
    // units and the final depth clamp do not depend on framebuffer dimensions.
    if (viewport_scale != 1.0f) parameters.slope *= viewport_scale;
    return parameters;
}

class DepthBiasScaleState {
public:
    bool Update(float viewport_scale, bool guest_or_command_buffer_dirty) noexcept {
        const bool changed = !valid || scale != viewport_scale;
        scale = viewport_scale;
        valid = true;
        return changed || guest_or_command_buffer_dirty;
    }

private:
    float scale{1.0f};
    bool valid{};
};

} // namespace Vulkan
