// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "video_core/renderer_vulkan/fixed_pipeline_state.h"

namespace Vulkan {

inline bool CanPrepareStaticLogicOp(const DynamicFeatures& features) {
    return features.has_extended_dynamic_state_2 &&
           !features.has_extended_dynamic_state_2_logic_op &&
           !features.has_extended_dynamic_state_3_enables &&
           !features.has_dynamic_state3_logic_op_enable;
}

inline void NormalizePreparedLogicOp(FixedPipelineState& state, const DynamicFeatures& features) {
    // A record with dynamic logic-op enable does not tell us whether the op was on.
    // A disabled static op has no effect, even when the recorded operation is unknown.
    if (CanPrepareStaticLogicOp(features) && state.extended_dynamic_state_2 &&
        !state.extended_dynamic_state_3_enables && !state.dynamic_state.logic_op_enable) {
        state.extended_dynamic_state_2_logic_op.Assign(0);
        state.dynamic_state.logic_op.Assign(0);
    }
}

} // namespace Vulkan
