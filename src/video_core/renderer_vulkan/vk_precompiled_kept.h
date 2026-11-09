// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Keeps shader metadata and SPIR-V from preparation. The append-only file can resume after
// interruption. Runtime holds the index in memory and reads each shader blob when needed.
//
// Layout: "JNX2SPV1", u32 stamp size, the stamp, then entries: u8 1 (info) or 2 (SPIR-V, one
// zstd frame of its words), u64 content hash, u32 size, the bytes; u8 3 (record), u32 record
// index in the precompiled file, then a Record (72 bytes); u8 4 ends the entries. Then the
// index: u32 graphics count, u32 compute count, u32 blob count, the Records of each kind sorted
// by key hash, per blob u64 offset, u32 size, u32 0, then per Record (graphics, then compute)
// the u64 offset of its record in the precompiled file. Last, u64 index offset and "NX2KEPTE".
// Blob ids number the info and SPIR-V blobs together, in the order they were written.
//
// The stamp covers the source preparation, translator, GPU, driver, and relevant settings.

#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"

namespace Shader {
struct Info;
}

namespace VideoCommon {
class FileEnvironment;
}

namespace Vulkan::PrecompiledKept {

// An OS-owned lock releases automatically when a preparation is canceled or the process exits.
class PreparationLock {
public:
    explicit PreparationLock(const std::filesystem::path& path);
    ~PreparationLock();
    PreparationLock(const PreparationLock&) = delete;
    PreparationLock& operator=(const PreparationLock&) = delete;
    [[nodiscard]] bool IsLocked() const noexcept;

private:
#ifdef _WIN32
    void* handle;
#else
    int fd{-1};
#endif
};

constexpr u32 NoBlob = ~0U;

/// Record flags.
constexpr u8 FlagLibraries = 1; ///< Emitted for graphics pipeline libraries (one set per stage)

struct Record {
    u64 key_hash;
    /// The precompiled file's environments, in stage order (env_count of them)
    std::array<u32, 5> envs;
    /// Per pipeline stage (vertex B to fragment; a compute pipeline's is stage 0), NoBlob for none
    std::array<u32, 5> infos;
    std::array<u32, 5> spirv;
    u8 env_count;
    u8 kind; ///< 0 graphics, 1 compute
    u8 flags;
    u8 reserved;
};
static_assert(sizeof(Record) == 72 && std::is_trivially_copyable_v<Record> &&
              std::has_unique_object_representations_v<Record>);

/// Fingerprint of the running executable's size and modification time.
u64 ExecutableStamp();

/// A file read with positional reads, safe from several threads at once; opened so the file can
/// still be renamed or deleted while it is open.
class PositionalFile {
public:
    explicit PositionalFile(const std::filesystem::path& path);
    ~PositionalFile();
    PositionalFile(const PositionalFile&) = delete;
    PositionalFile& operator=(const PositionalFile&) = delete;

    [[nodiscard]] bool IsOpen() const;
    /// Reads out.size() bytes at offset; false when the file ends first or the read fails.
    [[nodiscard]] bool ReadAt(u64 offset, std::span<u8> out) const;

private:
#ifdef _WIN32
    void* handle;
#else
    int fd{-1};
#endif
};

/// The run time index of a kept file. Reads are thread-safe (the GPU thread checks and creates,
/// pipeline workers build from the SPIR-V); a pipeline's build holds the index it was made from.
class Index {
public:
    /// The kept file `kept` written for `stamp` and the environment index of `precompiled`
    /// (cache version `cache_version`); null with `why` set when either does not match.
    static std::shared_ptr<const Index> Load(const std::filesystem::path& kept,
                                             const std::filesystem::path& precompiled,
                                             const std::string& stamp, u32 cache_version,
                                             std::string& why);

    /// The records of a key (a compute key has one per storage pointer state it was prepared in).
    [[nodiscard]] std::span<const Record> Graphics(u64 key_hash) const;
    [[nodiscard]] std::span<const Record> Compute(u64 key_hash) const;

    [[nodiscard]] bool ReadInfo(u32 blob, Shader::Info& info) const;
    [[nodiscard]] bool ReadSpirv(u32 blob, std::vector<u32>& words) const;
    [[nodiscard]] bool ReadEnvironment(u32 env, VideoCommon::FileEnvironment& out) const;
    [[nodiscard]] size_t GraphicsCount() const noexcept {
        return graphics.size();
    }
    [[nodiscard]] size_t ComputeCount() const noexcept {
        return compute.size();
    }

    Index(const std::filesystem::path& kept, const std::filesystem::path& precompiled);

private:
    [[nodiscard]] bool ReadBlob(u32 blob, std::vector<u8>& bytes) const;

    std::vector<Record> graphics; ///< Sorted by key hash
    std::vector<Record> compute;  ///< Sorted by key hash
    std::vector<std::pair<u64, u32>> blobs;        ///< Offset, size
    std::vector<std::pair<u64, u64>> environments; ///< The precompiled file's: offset, size
    PositionalFile kept_file;
    PositionalFile precompiled_file;
};

bool ReadPipelineEnvironment(const PositionalFile& file, u64 offset, u64 size,
                             VideoCommon::FileEnvironment& out);

/// Writes a kept file while the preparation runs (several workers at once).
class Writer {
public:
    /// Starts `part` for `stamp`, or with `resume` continues it when it was started for the same
    /// stamp (the entries up to the last complete one are kept). `total`: the precompiled
    /// file's record count. False when the file cannot be written.
    bool Open(const std::filesystem::path& part, const std::string& stamp, bool resume,
              size_t total);
    /// Records kept from an earlier, interrupted preparation.
    [[nodiscard]] size_t Resumed() const noexcept {
        return resumed;
    }

    /// Keeps one built pipeline: per stage its serialized info and its SPIR-V (empty for none).
    void Keep(size_t index, u8 kind, u8 flags, u64 key_hash, std::span<const u32> envs,
              const std::array<std::vector<u8>, 5>& infos,
              const std::array<std::vector<u32>, 5>& spirv);

    /// Flushes the log (before a checkpoint is written).
    [[nodiscard]] bool Flush();
    /// Writes the index and closes the file; false when it could not be written.
    /// `record_offsets`: where each record of the precompiled file starts.
    bool Finish(std::span<const u64> record_offsets);
    /// Closes the file as it is (an interrupted preparation), to be resumed.
    void Close();

    struct Totals {
        size_t records{};
        size_t infos{};
        size_t spirv{};
        u64 spirv_bytes{}; ///< Compressed
        u64 file_bytes{};
    };
    [[nodiscard]] Totals GetTotals();

private:
    bool Scan(const std::filesystem::path& part, const std::string& stamp, u64& valid_end);
    /// The kept records of each kind sorted by key hash, and their precompiled record offsets
    /// (graphics, then compute). Under the mutex.
    void SortedRecords(std::span<const u64> record_offsets, std::vector<Record>& graphics,
                       std::vector<Record>& compute, std::vector<u64>& offsets) const;
    u32 KeepBlob(u8 type, u64 hash, std::span<const u8> bytes);

    std::mutex mutex;
    std::ofstream file;
    u64 offset{};
    std::unordered_map<u64, u32> infos_by_hash;
    std::unordered_map<u64, u32> spirv_by_hash;
    std::vector<std::pair<u64, u32>> blobs;
    std::vector<Record> records; ///< By record index; key_hash 0 and env_count 0 when not kept
    std::vector<u8> present;
    size_t kept{};
    size_t resumed{};
    u64 spirv_bytes{};
    bool failed{};
};

} // namespace Vulkan::PrecompiledKept
