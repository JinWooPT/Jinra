// Explicit member keys avoid padding and reject mutable compiler-context changes.
#pragma once
#include <bit>
#include "common/settings.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/host_translate_info.h"
#include "vk_task_shader_reuse.h"
namespace Vulkan::TaskShaderReuse {
inline std::vector<u32> CompilerContextKey(const Shader::Profile& p, const Shader::HostTranslateInfo& h) {
    std::vector<u32> key;
    const auto add = [&](auto value) {
        if constexpr (sizeof(value)>4) { key.push_back(static_cast<u32>(value)); key.push_back(static_cast<u32>(value>>32)); }
        else key.push_back(static_cast<u32>(value));
    };
    add(p.supported_spirv);
    add(p.unified_descriptor_binding);
    add(p.support_descriptor_aliasing);
    add(p.support_int8);
    add(p.support_uniform_and_storage_buffer_8bit);
    add(p.support_storage_buffer_8bit);
    add(p.support_int16);
    add(p.support_uniform_and_storage_buffer_16bit);
    add(p.support_storage_buffer_16bit);
    add(p.support_int64);
    add(p.support_vertex_instance_id);
    add(p.support_float_controls);
    add(p.support_separate_denorm_behavior);
    add(p.support_separate_rounding_mode);
    add(p.support_fp16_denorm_preserve);
    add(p.support_fp32_denorm_preserve);
    add(p.support_fp16_denorm_flush);
    add(p.support_fp32_denorm_flush);
    add(p.support_fp16_signed_zero_nan_preserve);
    add(p.support_fp32_signed_zero_nan_preserve);
    add(p.support_fp64_signed_zero_nan_preserve);
    add(p.support_explicit_workgroup_layout);
    add(p.support_workgroup_layout_8bit_access);
    add(p.support_workgroup_layout_16bit_access);
    add(p.support_shader_quad_control);
    add(p.support_quad_shuffles);
    add(p.support_vote);
    add(p.support_shuffle_relative);
    add(p.supported_subgroup_stages);
    add(p.support_viewport_index_layer_non_geometry);
    add(p.support_viewport_mask);
    add(p.support_typeless_image_loads);
    add(p.support_demote_to_helper_invocation);
    add(p.support_int64_atomics);
    add(p.support_shared_int64_atomics);
    add(p.support_derivative_control);
    add(p.support_geometry_shader_passthrough);
    add(p.support_native_ndc);
    add(p.support_gl_nv_gpu_shader_5);
    add(p.support_gl_amd_gpu_shader_half_float);
    add(p.support_gl_texture_shadow_lod);
    add(p.support_gl_warp_intrinsics);
    add(p.support_gl_variable_aoffi);
    add(p.support_gl_sparse_textures);
    add(p.support_gl_derivative_control);
    add(p.support_scaled_attributes);
    add(p.support_multi_viewport);
    add(p.support_geometry_streams);
    add(p.support_sampled_image_array_nonuniform_indexing);
    add(p.support_storage_image_array_nonuniform_indexing);
    add(p.support_uniform_texel_buffer_array_nonuniform_indexing);
    add(p.support_storage_texel_buffer_array_nonuniform_indexing);
    add(p.warp_size_potentially_larger_than_guest);
    add(p.lower_left_origin_mode);
    add(p.need_declared_frag_colors);
    add(p.need_fastmath_off);
    add(p.need_gather_subpixel_offset);
    add(p.has_broken_spirv_clamp);
    add(p.has_broken_spirv_position_input);
    add(p.has_broken_unsigned_image_offsets);
    add(p.has_broken_signed_operations);
    add(p.has_broken_fp16_float_controls);
    add(p.has_broken_fp32_denorm_flush);
    add(p.has_gl_component_indexing_bug);
    add(p.has_gl_precise_bug);
    add(p.has_gl_cbuf_ftou_bug);
    add(p.has_gl_bool_ref_bug);
    add(p.ignore_nan_fp_comparisons);
    add(p.has_broken_spirv_subgroup_mask_vector_extract_dynamic);
    add(p.has_broken_spirv_subgroup_shuffle);
    add(p.max_subgroup_size);
    add(p.gl_max_compute_smem_size);
    add(p.has_broken_robust);
    add(p.min_ssbo_alignment);
    add(p.max_user_clip_distances);
    add(h.min_ssbo_alignment);
    add(h.max_per_stage_descriptor_sampled_images);
    add(h.max_per_stage_resources);
    add(h.max_descriptor_set_samplers);
    add(h.max_descriptor_set_uniform_buffers);
    add(h.max_descriptor_set_uniform_buffers_dynamic);
    add(h.max_descriptor_set_storage_buffers);
    add(h.max_descriptor_set_storage_buffers_dynamic);
    add(h.max_descriptor_set_sampled_images);
    add(h.max_descriptor_set_storage_images);
    add(h.max_descriptor_set_input_attachements);
    add(h.support_float64);
    add(h.support_float16);
    add(h.support_int64);
    add(h.needs_demote_reorder);
    add(h.support_snorm_render_buffer);
    add(h.support_viewport_index_layer);
    add(h.support_viewport_mask);
    add(h.support_geometry_shader_passthrough);
    add(h.support_conditional_barrier);
    add(h.single_lane_geometry_subgroups);
    add(h.support_buffer_texture_handles);
    add(Settings::GetCurrentProgramID());
    const auto& r=Settings::values.resolution_info;
    add(r.active); add(r.up_scale); add(r.down_shift);
    add(std::bit_cast<u32>(r.up_factor)); add(std::bit_cast<u32>(r.down_factor));
    add(Settings::values.renderer_debug.GetValue());
    add(Settings::values.disable_shader_loop_safety_checks.GetValue());
    add(Settings::values.fix_bloom_effects.GetValue());
    return key;
}
} // namespace Vulkan::TaskShaderReuse
