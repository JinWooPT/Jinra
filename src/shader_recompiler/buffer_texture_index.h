// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <bit>
#include <optional>
#include <span>
#include <vector>
#include "common/common_types.h"

namespace Shader {
enum class TextureIndexOp : u32 {
    Immediate, Cbuf, Add, Sub, Mul, And, Or, Xor, Shl, Shr, Sar,
    Eq, Ne, ULt, ULe, SLt, SLe, UMin, UMax, SMin, SMax,
    Neg, Not, LNot, LAnd, LOr, LXor, Select, ExtractU, ExtractS,
};
struct TextureIndexNode {
    TextureIndexOp op;
    u32 a{}, b{}, c{};
    auto operator<=>(const TextureIndexNode&) const = default;
};
using TextureIndexProgram = std::vector<TextureIndexNode>;

// Only draw-uniform integer expressions are admitted by the compiler. Evaluate
// selects lazily so an unused constant-buffer branch need not be bound.
template <typename ReadCbuf>
std::optional<u32> EvaluateTextureIndex(std::span<const TextureIndexNode> program, ReadCbuf&& read) {
    if (program.empty() || program.size() > 256) return {};
    std::array<u32, 256> values{};
    std::array<u8, 256> states{};
    const auto evaluate = [&](auto&& self, u32 index, u32 depth) -> std::optional<u32> {
        if (index >= program.size() || depth > 64 || states[index] == 1) return {};
        if (states[index] == 2) return values[index];
        states[index] = 1;
        const auto& node = program[index];
        const auto result = [&]() -> std::optional<u32> {
            if (node.op == TextureIndexOp::Immediate) return node.a;
            if (node.op == TextureIndexOp::Cbuf) {
                const auto offset = self(self, node.b, depth + 1);
                return offset ? read(node.a, *offset) : std::nullopt;
            }
            const auto a = self(self, node.a, depth + 1);
            if (!a) return {};
            switch (node.op) {
            case TextureIndexOp::Neg: return 0U - *a;
            case TextureIndexOp::Not: return ~*a;
            case TextureIndexOp::LNot: return *a == 0;
            case TextureIndexOp::Select: return self(self, *a ? node.b : node.c, depth + 1);
            default: break;
            }
            const auto b = self(self, node.b, depth + 1);
            if (!b) return {};
            const s32 sa = std::bit_cast<s32>(*a), sb = std::bit_cast<s32>(*b);
            switch (node.op) {
            case TextureIndexOp::Add: return *a + *b;
            case TextureIndexOp::Sub: return *a - *b;
            case TextureIndexOp::Mul: return *a * *b;
            case TextureIndexOp::And: return *a & *b;
            case TextureIndexOp::Or: return *a | *b;
            case TextureIndexOp::Xor: return *a ^ *b;
            case TextureIndexOp::Shl: return *b < 32 ? *a << *b : 0;
            case TextureIndexOp::Shr: return *b < 32 ? *a >> *b : 0;
            case TextureIndexOp::Sar: return static_cast<u32>(sa >> (*b < 32 ? *b : 31));
            case TextureIndexOp::Eq: return *a == *b;
            case TextureIndexOp::Ne: return *a != *b;
            case TextureIndexOp::ULt: return *a < *b;
            case TextureIndexOp::ULe: return *a <= *b;
            case TextureIndexOp::SLt: return sa < sb;
            case TextureIndexOp::SLe: return sa <= sb;
            case TextureIndexOp::UMin: return *a < *b ? *a : *b;
            case TextureIndexOp::UMax: return *a > *b ? *a : *b;
            case TextureIndexOp::SMin: return static_cast<u32>(sa < sb ? sa : sb);
            case TextureIndexOp::SMax: return static_cast<u32>(sa > sb ? sa : sb);
            case TextureIndexOp::LAnd: return *a != 0 && *b != 0;
            case TextureIndexOp::LOr: return *a != 0 || *b != 0;
            case TextureIndexOp::LXor: return (*a != 0) != (*b != 0);
            case TextureIndexOp::ExtractU:
            case TextureIndexOp::ExtractS: {
                const auto width = self(self, node.c, depth + 1);
                if (!width || *b >= 32 || *width > 32 - *b) return {};
                if (!*width) return 0;
                const u32 mask = *width == 32 ? ~0U : (1U << *width) - 1;
                const u32 field = (*a >> *b) & mask;
                if (node.op == TextureIndexOp::ExtractU) return field;
                const u32 sign = 1U << (*width - 1);
                return (field ^ sign) - sign;
            }
            default: return {};
            }
        }();
        if (result) { states[index] = 2; values[index] = *result; }
        return result;
    };
    return evaluate(evaluate, static_cast<u32>(program.size() - 1), 0);
}
} // namespace Shader
