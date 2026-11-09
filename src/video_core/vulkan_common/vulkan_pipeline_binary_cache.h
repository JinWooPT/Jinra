// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>

#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

namespace PipelineBinaryCache {

enum class PersistenceMode { Off, Auto, VulkanCache };
PersistenceMode GetPersistenceMode() noexcept;

/// Starts saving and reusing binaries for the pipelines created on this device. No-op when the
/// device doesn't have VK_KHR_pipeline_binary enabled.
bool Activate(VkDevice device, const vk::DeviceDispatch& dld, const std::filesystem::path& file);

/// Stops it and closes the file. Pipelines created afterwards are built normally.
void Deactivate(VkDevice device);

/// Called by vk::Device. Returns false when the pipeline should be created the usual way.
bool CreateGraphicsPipeline(VkDevice device, const vk::DeviceDispatch& dld,
                            const VkGraphicsPipelineCreateInfo& ci, VkPipelineCache cache,
                            VkPipeline* out, VkResult* result);
bool CreateComputePipeline(VkDevice device, const vk::DeviceDispatch& dld,
                           const VkComputePipelineCreateInfo& ci, VkPipelineCache cache,
                           VkPipeline* out, VkResult* result);

} // namespace PipelineBinaryCache

} // namespace Vulkan
