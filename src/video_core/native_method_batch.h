// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "common/common_types.h"

namespace VideoCommon {
template <class Engine, class System>
void EmitNativeMethod(Engine& engine, System& system, u32 method, u32 value,
                      u32 shadow_control, bool batch) {
    if (batch && method != shadow_control && !engine.execution_mask[method]) {
        engine.method_sink.emplace_back(method, value);
        return;
    }
    if (batch) engine.ConsumeSink(system);
    engine.CallMethod(system, method, value, true);
}

template <class Engine, class System>
u32 ReadNativeMethodRegister(Engine& engine, System& system, u32 method, bool batch) {
    if (batch) engine.ConsumeSink(system);
    return engine.GetRegisterValue(method);
}
} // namespace VideoCommon
