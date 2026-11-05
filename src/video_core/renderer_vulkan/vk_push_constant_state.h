// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <vulkan/vulkan.h>

namespace Vulkan {
constexpr VkShaderStageFlags PushConstantStages(VkShaderStageFlags shader_stages) {
    return shader_stages & VK_SHADER_STAGE_COMPUTE_BIT
               ? VkShaderStageFlags{VK_SHADER_STAGE_COMPUTE_BIT}
               : VK_SHADER_STAGE_ALL_GRAPHICS | shader_stages;
}

class RescalingPushConstantState {
public:
    bool Update(bool rescaling, VkShaderStageFlags shader_stages) {
        if (defined && rescaling == is_rescaling && shader_stages == stages) return false;
        defined = true;
        is_rescaling = rescaling;
        stages = shader_stages;
        return true;
    }

    void Invalidate() {
        defined = false;
    }

private:
    bool defined{};
    bool is_rescaling{};
    VkShaderStageFlags stages{};
};
} // namespace Vulkan
