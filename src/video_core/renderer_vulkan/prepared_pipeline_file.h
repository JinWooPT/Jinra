// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

#include "common/common_types.h"

namespace Vulkan {

// Indexed NX2PREP1 output from the NX2 recipe assembler. The game supplies the code in the file.
class PreparedPipelineFile {
public:
    struct Environment {
        u64 offset;
        u64 size;
    };

    struct Record {
        u8 kind;
        u8 count;
        u16 reserved;
        std::array<u32, 6> ids;

        bool IsCompute() const noexcept {
            return kind == 1;
        }
    };

    static constexpr size_t GraphicsKeySize = 1056;
    static constexpr size_t ComputeKeySize = 24;

    PreparedPipelineFile(std::ifstream& stream, u64 file_size, u32 expected_version) {
        std::array<char, 8> magic{};
        stream.read(magic.data(), magic.size());
        const auto version = Read<u32>(stream);
        const auto env_count = Read<u32>(stream);
        const auto record_count = Read<u32>(stream);
        const auto reserved = Read<u32>(stream);
        Require(magic == std::array{'N', 'X', '2', 'P', 'R', 'E', 'P', '1'} &&
                version == expected_version && reserved == 0);
        Require(env_count > 0 && env_count <= 1'000'000 && record_count > 0 &&
                record_count <= 1'000'000 && file_size >= 24 &&
                u64{env_count} * sizeof(Environment) <= file_size - 24);

        environments.resize(env_count);
        stream.read(reinterpret_cast<char*>(environments.data()), env_count * sizeof(Environment));
        u64 first_env = file_size;
        for (const auto& env : environments) {
            Require(env.offset <= file_size && env.size <= file_size - env.offset &&
                    env.size > 0 && env.size <= 4 * 1024 * 1024);
            first_env = std::min(first_env, env.offset);
        }

        Require(static_cast<u64>(stream.tellg()) <= first_env);
        records.reserve(record_count);
        for (u32 i = 0; i < record_count; ++i) {
            records.push_back(static_cast<u64>(stream.tellg()));
            const auto record = ReadRecord(stream);
            stream.seekg(record.IsCompute() ? ComputeKeySize : GraphicsKeySize, std::ios::cur);
            Require(static_cast<u64>(stream.tellg()) <= first_env);
        }
    }

    size_t Size() const noexcept {
        return records.size();
    }

    std::span<const u64> RecordOffsets() const noexcept {
        return records;
    }

    Record ReadRecord(std::ifstream& stream, size_t index) const {
        stream.seekg(static_cast<std::streamoff>(records[index]));
        return ReadRecord(stream);
    }

    const Environment& GetEnvironment(u32 id) const noexcept {
        return environments[id];
    }

    template <typename T>
    static T Read(std::ifstream& stream) {
        T data{};
        stream.read(reinterpret_cast<char*>(&data), sizeof(T));
        return data;
    }

private:
    Record ReadRecord(std::ifstream& stream) const {
        Record record{};
        record.kind = Read<u8>(stream);
        record.count = Read<u8>(stream);
        record.reserved = Read<u16>(stream);
        Require(record.kind <= 1 && record.count > 0 && record.count <= record.ids.size() &&
                record.reserved == 0 && (!record.IsCompute() || record.count == 1));
        stream.read(reinterpret_cast<char*>(record.ids.data()), record.count * sizeof(u32));
        Require(std::all_of(record.ids.begin(), record.ids.begin() + record.count,
                            [this](u32 id) { return id < environments.size(); }));
        return record;
    }

    static void Require(bool condition) {
        if (!condition) {
            throw std::runtime_error("Invalid or incompatible NX2PREP1 file");
        }
    }

    static_assert(sizeof(Environment) == 16);
    std::vector<Environment> environments;
    std::vector<u64> records;
};

} // namespace Vulkan
