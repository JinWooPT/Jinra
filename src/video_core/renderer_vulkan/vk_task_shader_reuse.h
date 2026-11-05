// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <vector>
#include <boost/container/small_vector.hpp>
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/runtime_info.h"

namespace Vulkan::TaskShaderReuse {

using Answer = boost::container::small_vector<u32, 4>;
enum class Query : u8 {
    Instruction, RegisterCopies, ProgramAddress, StoragePointer, Cbuf, TextureType,
    TextureFormat, TextureInteger, Viewport, BoundBuffer, Local, Shared, Workgroup,
    HLE, Replacement, Native, NativeProgram, Indirect, IndirectAt, SourceBase,
};
struct ReadKey {
    Query query;
    u32 a{}, b{};
    auto operator<=>(const ReadKey&) const = default;
};

template <class T>
Answer Encode(T value) { return {static_cast<u32>(value)}; }
inline Answer Encode(u64 value) { return {static_cast<u32>(value), static_cast<u32>(value >> 32)}; }
inline Answer Encode(std::array<u32, 3> value) { return {value.begin(), value.end()}; }
inline Answer Encode(std::optional<Shader::ReplaceConstant> value) {
    return value ? Answer{1, static_cast<u32>(*value)} : Answer{0};
}
inline Answer Encode(std::span<const u32> value) { return {value.begin(), value.end()}; }
inline Answer Encode(std::span<const Shader::Ampere::Instruction> value) {
    Answer answer;
    answer.reserve(value.size() * 4);
    for (const auto& instruction : value) {
        answer.insert(answer.end(), instruction.words.begin(), instruction.words.end());
    }
    return answer;
}

struct Properties {
    std::array<u32, sizeof(Shader::ProgramHeader) / sizeof(u32)> header{}, primitive_header{};
    std::array<u32, 8> passthrough{};
    std::array<u32, 3> mesh_registers{};
    Shader::Stage stage{};
    u32 start{};
    bool proprietary{};
    explicit Properties(const Shader::Environment& env)
        : passthrough{env.GpPassthroughMask()}, mesh_registers{env.MeshRegisters()},
          stage{env.ShaderStage()}, start{env.StartAddress()}, proprietary{env.IsProprietaryDriver()} {
        static_assert(sizeof(Shader::ProgramHeader) % sizeof(u32) == 0);
        std::memcpy(header.data(), &env.SPH(), sizeof(Shader::ProgramHeader));
        std::memcpy(primitive_header.data(), &env.MeshPrimitiveHeader(), sizeof(Shader::ProgramHeader));
    }
    bool operator==(const Properties&) const = default;
};

inline Answer Read(Shader::Environment& env, ReadKey key) {
    switch (key.query) {
    case Query::Instruction: return Encode(env.ReadInstruction(key.a));
    case Query::RegisterCopies: return Encode(env.HasNativeRegisterCopies());
    case Query::ProgramAddress: return Encode(env.TranslateProgramAddress(key.a));
    case Query::StoragePointer: return Encode(env.IsCbufStorageBufferPointer(key.a, key.b));
    case Query::Cbuf: return Encode(env.ReadCbufValue(key.a, key.b));
    case Query::TextureType: return Encode(env.ReadTextureType(key.a));
    case Query::TextureFormat: return Encode(env.ReadTexturePixelFormat(key.a));
    case Query::TextureInteger: return Encode(env.IsTexturePixelFormatInteger(key.a));
    case Query::Viewport: return Encode(env.ReadViewportTransformState());
    case Query::BoundBuffer: return Encode(env.TextureBoundBuffer());
    case Query::Local: return Encode(env.LocalMemorySize());
    case Query::Shared: return Encode(env.SharedMemorySize());
    case Query::Workgroup: return Encode(env.WorkgroupSize());
    case Query::HLE: return Encode(env.HasHLEMacroState());
    case Query::Replacement: return Encode(env.GetReplaceConstBuffer(key.a, key.b));
    case Query::Native: return Encode(env.IsNativeAmpere());
    case Query::NativeProgram: return Encode(env.NativeProgram());
    case Query::Indirect: return Encode(env.NativeIndirectTargets());
    case Query::IndirectAt: return Encode(env.NativeIndirectTargetsAt(key.a));
    case Query::SourceBase: return Encode(env.NativeSourceBase());
    }
    return {};
}

struct Ledger {
    static constexpr size_t MaxReads = 16384;
    static constexpr size_t MaxWords = 262144;
    Properties properties;
    std::map<ReadKey, Answer> reads;
    size_t words{};
    bool stable{true};
    explicit Ledger(const Shader::Environment& env) : properties{env} {}
    bool Matches(Shader::Environment& env) const {
        if (!stable || properties != Properties{env}) return false;
        try {
            for (const auto& [key, expected] : reads) {
                if (Read(env, key) != expected) return false;
            }
            return true;
        } catch (...) {
            // A new failure is a cache miss; normal compilation decides how to handle it.
            return false;
        }
    }
};

// Every compiler-visible virtual is forwarded. Never eagerly call an unused virtual: some
// environments legitimately throw for unsupported queries. Nonvirtual properties are cloned.
class RecordingEnvironment final : public Shader::Environment {
public:
    explicit RecordingEnvironment(Shader::Environment& inner_) : inner{inner_}, ledger{inner_} {
        sph = inner.SPH();
        gp_passthrough_mask = inner.GpPassthroughMask();
        mesh_registers = inner.MeshRegisters();
        mesh_primitive_header = inner.MeshPrimitiveHeader();
        stage = inner.ShaderStage();
        start_address = inner.StartAddress();
        is_proprietary_driver = inner.IsProprietaryDriver();
    }
    u64 ReadInstruction(u32 a) override { return Get({Query::Instruction,a}, [&] { return inner.ReadInstruction(a); }); }
    bool HasNativeRegisterCopies() const override { return Get({Query::RegisterCopies}, [&] { return inner.HasNativeRegisterCopies(); }); }
    u32 TranslateProgramAddress(u32 a) const override { return Get({Query::ProgramAddress,a}, [&] { return inner.TranslateProgramAddress(a); }); }
    bool IsCbufStorageBufferPointer(u32 a,u32 b) override { return Get({Query::StoragePointer,a,b}, [&] { return inner.IsCbufStorageBufferPointer(a,b); }); }
    u32 ReadCbufValue(u32 a,u32 b) override { return Get({Query::Cbuf,a,b}, [&] { return inner.ReadCbufValue(a,b); }); }
    Shader::TextureType ReadTextureType(u32 a) override { return Get({Query::TextureType,a}, [&] { return inner.ReadTextureType(a); }); }
    Shader::TexturePixelFormat ReadTexturePixelFormat(u32 a) override { return Get({Query::TextureFormat,a}, [&] { return inner.ReadTexturePixelFormat(a); }); }
    bool IsTexturePixelFormatInteger(u32 a) override { return Get({Query::TextureInteger,a}, [&] { return inner.IsTexturePixelFormatInteger(a); }); }
    u32 ReadViewportTransformState() override { return Get({Query::Viewport}, [&] { return inner.ReadViewportTransformState(); }); }
    u32 TextureBoundBuffer() const override { return Get({Query::BoundBuffer}, [&] { return inner.TextureBoundBuffer(); }); }
    u32 LocalMemorySize() const override { return Get({Query::Local}, [&] { return inner.LocalMemorySize(); }); }
    u32 SharedMemorySize() const override { return Get({Query::Shared}, [&] { return inner.SharedMemorySize(); }); }
    std::array<u32,3> WorkgroupSize() const override { return Get({Query::Workgroup}, [&] { return inner.WorkgroupSize(); }); }
    bool HasHLEMacroState() const override { return Get({Query::HLE}, [&] { return inner.HasHLEMacroState(); }); }
    std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32 a,u32 b) override { return Get({Query::Replacement,a,b}, [&] { return inner.GetReplaceConstBuffer(a,b); }); }
    bool IsNativeAmpere() const override { return Get({Query::Native}, [&] { return inner.IsNativeAmpere(); }); }
    std::span<const Shader::Ampere::Instruction> NativeProgram() const override { return Get({Query::NativeProgram}, [&] { return inner.NativeProgram(); }); }
    std::span<const u32> NativeIndirectTargets() const override { return Get({Query::Indirect}, [&] { return inner.NativeIndirectTargets(); }); }
    std::span<const u32> NativeIndirectTargetsAt(u32 a) const override { return Get({Query::IndirectAt,a}, [&] { return inner.NativeIndirectTargetsAt(a); }); }
    u32 NativeSourceBase() const override { return Get({Query::SourceBase}, [&] { return inner.NativeSourceBase(); }); }
    void Dump(u64 pipeline,u64 shader) override { inner.Dump(pipeline,shader); }
    Ledger Finish() && { return std::move(ledger); }
private:
    template <class F>
    auto Get(ReadKey key, F&& fn) const -> decltype(fn()) {
        try {
            auto value = fn();
            if (ledger.stable) {
                auto answer = Encode(value);
                const auto existing = ledger.reads.find(key);
                if (existing != ledger.reads.end()) {
                    if (existing->second != answer) ledger.stable = false;
                } else if (ledger.reads.size() >= Ledger::MaxReads ||
                           answer.size() > Ledger::MaxWords - ledger.words) {
                    ledger.stable = false;
                } else {
                    ledger.words += answer.size();
                    ledger.reads.emplace(key, std::move(answer));
                }
            }
            return value;
        } catch (...) {
            // Even a query exception caught by the compiler disqualifies this compilation.
            ledger.stable = false;
            throw;
        }
    }
    Shader::Environment& inner;
    mutable Ledger ledger;
};

inline std::array<u32,7> BindingKey(const Shader::Backend::Bindings& b) {
    return {b.unified,b.uniform_buffer,b.storage_buffer,b.texture,b.image,
            b.texture_scaling_index,b.image_scaling_index};
}
// No raw object hashing: optional, map and bitset object representations contain padding/pointers.
inline std::vector<u32> RuntimeKey(const Shader::RuntimeInfo& r) {
    std::vector<u32> key;
    const auto add = [&](auto v) { key.push_back(static_cast<u32>(v)); };
    for (auto value : r.generic_input_types) add(value);
    for (size_t bit=0; bit<r.previous_stage_stores.mask.size(); ++bit) add(r.previous_stage_stores.mask[bit]);
    for (size_t bit=0; bit<r.per_primitive_generics.size(); ++bit) add(r.per_primitive_generics[bit]);
    add(r.previous_stage_legacy_stores_mapping.size());
    for (const auto& [a,b] : r.previous_stage_legacy_stores_mapping) { add(a); add(b); }
    add(r.convert_depth_mode); add(r.force_early_z); add(r.tess_primitive); add(r.tess_spacing);
    add(r.tess_clockwise); add(r.input_topology);
    add(r.fixed_state_point_size.has_value());
    if (r.fixed_state_point_size) add(std::bit_cast<u32>(*r.fixed_state_point_size));
    add(r.alpha_test_func.has_value()); if (r.alpha_test_func) add(*r.alpha_test_func);
    add(std::bit_cast<u32>(r.alpha_test_reference)); add(r.y_negate); add(r.glasm_use_storage_buffers);
    for (const auto& v : r.xfb_varyings) { add(v.buffer); add(v.stream); add(v.stride); add(v.offset); add(v.components); }
    add(r.xfb_count);
    for (auto type : r.color_output_types) add(type);
    add(r.dual_source_blend);
    return key;
}

// Owns values only. No blocks, ASL nodes, or pool-backed IR survive between pipeline builds.
struct ProgramMetadata {
    Shader::Info info;
    Shader::Stage stage{};
    std::array<u32,3> workgroup_size{};
    Shader::OutputTopology output_topology{};
    u32 output_vertices{}, invocations{}, local_memory_size{}, shared_memory_size{};
    bool is_geometry_passthrough{};
    Shader::MeshInfo mesh;
    explicit ProgramMetadata(const Shader::IR::Program& p)
        : info{p.info}, stage{p.stage}, workgroup_size{p.workgroup_size}, output_topology{p.output_topology},
          output_vertices{p.output_vertices}, invocations{p.invocations}, local_memory_size{p.local_memory_size},
          shared_memory_size{p.shared_memory_size}, is_geometry_passthrough{p.is_geometry_passthrough}, mesh{p.mesh} {}
    Shader::IR::Program Restore() const {
        Shader::IR::Program p;
        p.info=info; p.stage=stage; p.workgroup_size=workgroup_size; p.output_topology=output_topology;
        p.output_vertices=output_vertices; p.invocations=invocations; p.local_memory_size=local_memory_size;
        p.shared_memory_size=shared_memory_size; p.is_geometry_passthrough=is_geometry_passthrough; p.mesh=mesh;
        return p;
    }
};
struct Artifact {
    u64 shader_hash{}; // candidate index only; complete context and environment still validated
    std::vector<u32> context, runtime, words;
    std::array<u32,7> initial_bindings{};
    Shader::Backend::Bindings final_bindings{};
    ProgramMetadata program;
    Ledger ledger;
    Artifact(u64 hash, std::vector<u32> ctx, const Shader::RuntimeInfo& r,
             const Shader::Backend::Bindings& before, const Shader::Backend::Bindings& after,
             std::vector<u32> code, const Shader::IR::Program& p, Ledger record)
        : shader_hash{hash}, context{std::move(ctx)}, runtime{RuntimeKey(r)}, words{std::move(code)},
          initial_bindings{BindingKey(before)}, final_bindings{after}, program{p}, ledger{std::move(record)} {}
    size_t Bytes() const {
        // Conservative accounting, including inline vectors already included in sizeof(*this).
        size_t bytes=sizeof(*this)+(context.capacity()+runtime.capacity()+words.capacity())*sizeof(u32);
        for (const auto& [key,answer] : ledger.reads) {
            bytes+=sizeof(ReadKey)+sizeof(Answer)+64+answer.capacity()*sizeof(u32);
        }
        const auto& i=program.info;
        bytes+=i.legacy_stores_mapping.size()*(sizeof(Shader::IR::Attribute)*2+64);
        bytes+=i.texture_buffer_descriptors.capacity()*sizeof(Shader::TextureBufferDescriptor);
        bytes+=i.image_buffer_descriptors.capacity()*sizeof(Shader::ImageBufferDescriptor);
        bytes+=i.texture_descriptors.capacity()*sizeof(Shader::TextureDescriptor);
        bytes+=i.image_descriptors.capacity()*sizeof(Shader::ImageDescriptor);
        for (const auto& d : i.texture_descriptors) {
            bytes+=d.buffer_source.uniform_index.capacity()*sizeof(Shader::TextureIndexNode);
        }
        return bytes;
    }
};

// One instance belongs to one PipelineCache/device/game. Access ONLY from runtime GPU thread;
// disk-preload workers never consult it. Returned shared ownership survives bounded eviction.
class Cache {
public:
    static constexpr size_t MaxEntries = 16, MaxBytes = 16*1024*1024;
    std::shared_ptr<const Artifact> Find(u64 hash, std::span<const u32> context,
                                      const Shader::RuntimeInfo& runtime,
                                      const Shader::Backend::Bindings& bindings,
                                      Shader::Environment& env) const {
        const auto r = RuntimeKey(runtime);
        for (auto it=entries.rbegin(); it!=entries.rend(); ++it) {
            const auto& a=**it;
            if (a.shader_hash==hash && std::ranges::equal(a.context,context) && a.runtime==r &&
                a.initial_bindings==BindingKey(bindings) && a.ledger.Matches(env)) return *it;
        }
        return {};
    }
    bool Insert(Artifact artifact) {
        if (!artifact.ledger.stable || artifact.program.stage!=Shader::Stage::Task ||
            !artifact.program.info.is_task || artifact.program.info.is_mesh ||
            artifact.program.info.requires_layer_emulation || artifact.words.empty()) return false;
        const auto bytes=artifact.Bytes();
        if (bytes>MaxBytes) return false;
        while (!entries.empty() && (entries.size()>=MaxEntries || retained+bytes>MaxBytes)) {
            retained-=entries.front()->Bytes(); entries.erase(entries.begin());
        }
        retained+=bytes;
        entries.emplace_back(std::make_shared<const Artifact>(std::move(artifact)));
        return true;
    }
    size_t Size() const { return entries.size(); }
    size_t Bytes() const { return retained; }
private:
    std::vector<std::shared_ptr<const Artifact>> entries;
    size_t retained{};
};
} // namespace Vulkan::TaskShaderReuse
