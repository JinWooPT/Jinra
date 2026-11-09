// SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <share.h>
#else
#include <sys/file.h>
#endif

#include "common/cityhash.h"
#include "common/logging.h"
#include "common/scope_exit.h"
#include "common/thread_worker.h"
#include "video_core/vulkan_common/vulkan_pipeline_binary_cache.h"

namespace Vulkan::PipelineBinaryCache {

PersistenceMode GetPersistenceMode() noexcept {
    static const auto mode = [] {
        const char* value = std::getenv("EDEN_NX2_PERSIST_PIPELINES");
        if (value && std::strcmp(value, "1") == 0) {
            return PersistenceMode::Auto;
        }
        return value && std::strcmp(value, "cache") == 0 ? PersistenceMode::VulkanCache
                                                        : PersistenceMode::Off;
    }();
    return mode;
}

namespace {

// Records share a header: type, key size, padded key, payload size, checksum.
// A binary holds the opaque driver bytes. A pipeline holds the ordered binary keys.
constexpr std::array<char, 8> FILE_MAGIC{'J', 'I', 'N', 'P', 'B', 'I', 'N', '3'};
constexpr u32 PIPELINE_RECORD = 0x44524350;
constexpr u32 BINARY_RECORD = 0x44524342;
constexpr size_t KEY_BYTES = VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR;
constexpr size_t PACKED_KEY_BYTES = sizeof(u32) + KEY_BYTES;
constexpr size_t HEADER_BYTES = FILE_MAGIC.size() + 4 + KEY_BYTES;
constexpr size_t RECORD_HEADER_BYTES = 4 + 4 + KEY_BYTES + 8 + 8;
constexpr u32 MAX_BINARIES_PER_PIPELINE = 64;

constexpr u64 MAX_FILE_BYTES = 256ULL << 20;
constexpr u64 MIN_DEAD_BYTES = 64ULL << 20;
constexpr u64 MAX_PAYLOAD_BYTES = 8ULL << 20;
constexpr u64 MAX_PENDING_BYTES = 16ULL << 20;

struct Stats {
    std::atomic<u64> loaded{}, compiled{}, saved{}, load_failed{}, skipped{};
    std::atomic<u64> loaded_us{}, compiled_us{};
};

class Store {
public:
    bool Open(VkDevice device_, const vk::DeviceDispatch& dld_, const std::filesystem::path& file) {
        device = device_;
        dld = &dld_;
        path = file;
        VkPipelineBinaryKeyKHR global{.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
        if (dld->vkGetPipelineKeyKHR(device, nullptr, &global) != VK_SUCCESS ||
            global.keySize == 0 || global.keySize > KEY_BYTES) {
            LOG_WARNING(Render_Vulkan, "Pipeline binaries: the driver gave no global key");
            return false;
        }
        std::memcpy(header.data(), FILE_MAGIC.data(), FILE_MAGIC.size());
        std::memcpy(header.data() + 8, &global.keySize, 4);
        std::memcpy(header.data() + 12, global.key, global.keySize);

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        auto lock_path = path;
        lock_path += ".lock";
#ifdef _WIN32
        file_lock.reset(_wfsopen(lock_path.c_str(), L"ab", _SH_DENYRW));
        if (!file_lock) {
            return false;
        }
#else
        file_lock.reset(std::fopen(lock_path.c_str(), "ab"));
        if (!file_lock || flock(fileno(file_lock.get()), LOCK_EX | LOCK_NB) != 0) {
            return false;
        }
#endif
        u64 size = std::filesystem::file_size(path, ec);
        if (ec) {
            size = 0;
        }
        if (size > MAX_FILE_BYTES) {
            return false;
        }
        bool same_driver = false;
        {
            std::ifstream scan(path, std::ios::binary);
            std::array<u8, HEADER_BYTES> found{};
            same_driver = scan && size >= HEADER_BYTES &&
                          scan.read(reinterpret_cast<char*>(found.data()), HEADER_BYTES) &&
                          found == header;
            if (same_driver) {
                file_end = HEADER_BYTES;
                std::array<u8, RECORD_HEADER_BYTES> record{};
                while (file_end + RECORD_HEADER_BYTES <= size &&
                       scan.read(reinterpret_cast<char*>(record.data()), RECORD_HEADER_BYTES)) {
                    u32 magic, key_size;
                    u64 len, checksum;
                    std::memcpy(&magic, &record[0], 4);
                    std::memcpy(&key_size, &record[4], 4);
                    std::memcpy(&len, &record[8 + KEY_BYTES], 8);
                    std::memcpy(&checksum, &record[16 + KEY_BYTES], 8);
                    if ((magic != PIPELINE_RECORD && magic != BINARY_RECORD) ||
                        key_size == 0 || key_size > KEY_BYTES || len == 0 ||
                        len > MAX_PAYLOAD_BYTES ||
                        len > size - file_end - RECORD_HEADER_BYTES) {
                        break;
                    }
                    const u64 payload = file_end + RECORD_HEADER_BYTES;
                    const std::string key(reinterpret_cast<const char*>(&record[8]), key_size);
                    if (magic == BINARY_RECORD) {
                        Remember(blobs, key, {payload, len, checksum});
                    } else if (len <= MAX_BINARIES_PER_PIPELINE * PACKED_KEY_BYTES &&
                               len % PACKED_KEY_BYTES == 0) {
                        Remember(index, key, {payload, len, checksum});
                    }
                    file_end = payload + len;
                    scan.seekg(static_cast<std::streamoff>(file_end));
                }
            }
        }
        if (same_driver) {
            if (file_end < size) {
                // A session that ended mid-write leaves a partial record; drop it.
                std::filesystem::resize_file(path, file_end, ec);
                if (ec) {
                    return false;
                }
            }
            out.open(path, std::ios::binary | std::ios::app);
        } else {
            // First run, or the driver changed: binaries from another driver can't be used, so the
            // file is replaced rather than kept beside the new one.
            if (size != 0) {
                LOG_INFO(Render_Vulkan, "Pipeline binaries: driver changed, starting over");
            }
            index.clear();
            dead_bytes = 0;
            out.open(path, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(header.data()), header.size());
            out.flush();
            file_end = HEADER_BYTES;
        }
        in.open(path, std::ios::binary);
        if (!out || !in) {
            LOG_ERROR(Render_Vulkan, "Pipeline binaries: can't open {}", path.string());
            return false;
        }
        LOG_INFO(Render_Vulkan,
                 "Pipeline binaries: {} saved pipelines, {} unique binaries "
                 "({:.1f} MiB, {:.1f} MiB replaced) in {}",
                 index.size(), blobs.size(), Mib(file_end), Mib(dead_bytes), path.string());
        return true;
    }

    void Close() {
        writer.WaitForRequests();
        std::scoped_lock lock{mutex};
        out.close();
        Compact();
        in.close();
        LOG_INFO(Render_Vulkan,
                 "Pipeline binaries: {} pipelines built from saved binaries (avg {:.2f} ms), {} "
                 "compiled (avg {:.2f} ms), {} saved, {} saved binaries rejected",
                 stats.loaded.load(), Average(stats.loaded_us, stats.loaded),
                 stats.compiled.load(), Average(stats.compiled_us, stats.compiled),
                 stats.saved.load(), stats.load_failed.load());
    }

    bool Owns(VkDevice candidate) const noexcept {
        return device == candidate;
    }

    template <typename CreateInfo, typename CreateFn>
    bool Create(const CreateInfo& ci, VkPipeline* out_pipeline,
                VkResult* result, CreateFn&& create) {
        if (!Eligible(ci)) {
            ++stats.skipped;
            return false;
        }
        const auto start = std::chrono::steady_clock::now();
        const auto elapsed_us = [&] {
            return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::steady_clock::now() - start)
                                        .count());
        };
        const VkPipelineCreateInfoKHR key_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR,
            .pNext = const_cast<CreateInfo*>(&ci),
        };
        VkPipelineBinaryKeyKHR key{.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
        if (dld->vkGetPipelineKeyKHR(device, &key_info, &key) != VK_SUCCESS || key.keySize == 0 ||
            key.keySize > KEY_BYTES) {
            ++stats.skipped;
            return false;
        }
        const std::string key_bytes(reinterpret_cast<const char*>(key.key), key.keySize);

        if (auto saved = Read(key_bytes)) {
            if (CreateFromSaved(*saved, ci, out_pipeline, create)) {
                MarkUsed(key_bytes);
                ++stats.loaded;
                stats.loaded_us += elapsed_us();
                *result = VK_SUCCESS;
                return true;
            }
            // Compiled and saved again below, replacing the rejected record.
            Forget(key_bytes);
            if (stats.load_failed++ < 5) {
                LOG_WARNING(Render_Vulkan, "Pipeline binaries: a saved pipeline was rejected, "
                                           "compiling it again");
            }
        }

        {
            std::scoped_lock lock{mutex};
            if (!out.is_open() || file_end + pending_bytes + RECORD_HEADER_BYTES >= MAX_FILE_BYTES) {
                return false;
            }
        }

        // Compile with capture, then keep the driver's binaries.
        CreateInfo capture_ci = ci;
        VkPipelineCreateFlags2CreateInfoKHR flags2{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR,
            .pNext = ci.pNext,
            .flags = static_cast<VkPipelineCreateFlags2KHR>(ci.flags) |
                     VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR,
        };
        capture_ci.pNext = &flags2;
        VkResult res = create(capture_ci, VK_NULL_HANDLE, out_pipeline);
        if (res != VK_SUCCESS) {
            if (res < 0 && res != VK_ERROR_DEVICE_LOST) {
                // Let the caller build it without capture and report its own errors.
                if (*out_pipeline != VK_NULL_HANDLE) {
                    dld->vkDestroyPipeline(device, *out_pipeline, nullptr);
                }
                *out_pipeline = VK_NULL_HANDLE;
                return false;
            }
            *result = res;
            return true;
        }
        ++stats.compiled;
        const VkReleaseCapturedPipelineDataInfoKHR release{
            .sType = VK_STRUCTURE_TYPE_RELEASE_CAPTURED_PIPELINE_DATA_INFO_KHR,
            .pipeline = *out_pipeline,
        };
        SCOPE_EXIT { dld->vkReleaseCapturedPipelineDataKHR(device, &release, nullptr); };
        if (elapsed_us() >= 2000) {
            Save(key_bytes, *out_pipeline);
        }
        stats.compiled_us += elapsed_us();
        *result = VK_SUCCESS;
        return true;
    }

private:
    struct Record {
        u64 offset;
        u64 size;
        u64 checksum;
        bool used{};
    };
    using RecordIndex = std::unordered_map<std::string, Record>;

    struct Binary {
        std::string key;
        std::vector<u8> data;
    };

    using BinaryData = std::shared_ptr<const std::vector<u8>>;

    struct SavedPipeline {
        std::vector<VkPipelineBinaryKeyKHR> keys;
        std::vector<BinaryData> data;
    };

    static double Mib(u64 bytes) {
        return static_cast<double>(bytes) / (1024.0 * 1024.0);
    }

    static double Average(const std::atomic<u64>& us, const std::atomic<u64>& count) {
        const u64 n = count.load();
        return n == 0 ? 0.0 : static_cast<double>(us.load()) / 1000.0 / static_cast<double>(n);
    }

    static const VkBaseInStructure* Find(const void* next, VkStructureType type) {
        for (auto* s = static_cast<const VkBaseInStructure*>(next); s; s = s->pNext) {
            if (s->sType == type) {
                return s;
            }
        }
        return nullptr;
    }

    template <typename CreateInfo>
    static bool Eligible(const CreateInfo& ci) {
        // Libraries, pipelines already built from binaries and callers that pass their own
        // VkPipelineCreateFlags2 are left alone.
        return (ci.flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) == 0 &&
               !Find(ci.pNext, VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR) &&
               !Find(ci.pNext, VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT) &&
               !Find(ci.pNext, VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR) &&
               !Find(ci.pNext, VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR);
    }

    // Callers hold the mutex, or own the store alone (Open).
    void Remember(RecordIndex& records, const std::string& key, const Record& record) {
        const auto [it, inserted] = records.try_emplace(key, record);
        if (!inserted) {
            dead_bytes += RECORD_HEADER_BYTES + it->second.size;
            it->second = record;
        }
    }

    bool ReadRecord(const Record& record, std::vector<u8>& payload) {
        payload.resize(static_cast<size_t>(record.size));
        in.clear();
        in.seekg(static_cast<std::streamoff>(record.offset));
        return in.read(reinterpret_cast<char*>(payload.data()),
                       static_cast<std::streamsize>(payload.size())) &&
               PayloadHash(payload.data(), payload.size()) == record.checksum;
    }

    static std::vector<std::string> UnpackKeys(const std::vector<u8>& payload) {
        std::vector<std::string> keys;
        for (size_t offset = 0; offset < payload.size(); offset += PACKED_KEY_BYTES) {
            u32 size;
            std::memcpy(&size, payload.data() + offset, sizeof(size));
            if (size == 0 || size > KEY_BYTES) {
                return {};
            }
            keys.emplace_back(reinterpret_cast<const char*>(payload.data() + offset + sizeof(size)),
                              size);
        }
        return keys;
    }

    void Erase(RecordIndex& records, RecordIndex::iterator it) {
        dead_bytes += RECORD_HEADER_BYTES + it->second.size;
        records.erase(it);
    }

    BinaryData ReadBlob(const std::string& key) {
        const auto it = blobs.find(key);
        if (it == blobs.end()) {
            return {};
        }
        const auto& record = it->second;
        if (record.offset == recent_blob_offset) {
            return recent_blob;
        }
        auto payload = std::make_shared<std::vector<u8>>();
        if (!ReadRecord(record, *payload)) {
            Erase(blobs, it);
            return {};
        }
        // Keep one large shared binary hot while nearby pipelines are being restored.
        if (record.size >= (256ULL << 10)) {
            recent_blob_offset = record.offset;
            recent_blob = payload;
        }
        return payload;
    }

    std::optional<SavedPipeline> Read(const std::string& key) {
        std::scoped_lock lock{mutex};
        const auto it = index.find(key);
        if (it == index.end()) {
            return std::nullopt;
        }
        std::vector<u8> payload;
        const auto keys = ReadRecord(it->second, payload) ? UnpackKeys(payload)
                                                         : std::vector<std::string>{};
        if (keys.empty()) {
            Erase(index, it);
            return std::nullopt;
        }
        SavedPipeline saved;
        u64 total_bytes = 0;
        for (const auto& binary_key : keys) {
            const auto found = blobs.find(binary_key);
            if (found == blobs.end() || found->second.size > MAX_PAYLOAD_BYTES - total_bytes) {
                Erase(index, it);
                return std::nullopt;
            }
            total_bytes += found->second.size;
            auto data = ReadBlob(binary_key);
            if (!data) {
                Erase(index, it);
                return std::nullopt;
            }
            VkPipelineBinaryKeyKHR vk_key{.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR,
                                          .keySize = static_cast<u32>(binary_key.size())};
            std::memcpy(vk_key.key, binary_key.data(), binary_key.size());
            saved.keys.push_back(vk_key);
            saved.data.push_back(std::move(data));
        }
        return saved;
    }

    void MarkUsed(const std::string& key) {
        std::scoped_lock lock{mutex};
        if (const auto it = index.find(key); it != index.end()) {
            it->second.used = true;
        }
    }

    void Forget(const std::string& key) {
        std::scoped_lock lock{mutex};
        if (const auto it = index.find(key); it != index.end()) {
            Erase(index, it);
        }
    }

    template <typename CreateInfo, typename CreateFn>
    bool CreateFromSaved(const SavedPipeline& saved, const CreateInfo& ci,
                         VkPipeline* out_pipeline, CreateFn& create) {
        const u32 count = static_cast<u32>(saved.keys.size());
        std::vector<VkPipelineBinaryDataKHR> datas;
        datas.reserve(count);
        for (const auto& data : saved.data) {
            datas.push_back({.dataSize = data->size(), .pData = const_cast<u8*>(data->data())});
        }
        const VkPipelineBinaryKeysAndDataKHR keys_and_data{
            .binaryCount = count,
            .pPipelineBinaryKeys = saved.keys.data(),
            .pPipelineBinaryData = datas.data(),
        };
        const VkPipelineBinaryCreateInfoKHR binary_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR,
            .pKeysAndDataInfo = &keys_and_data,
        };
        std::vector<VkPipelineBinaryKHR> binaries(count, VK_NULL_HANDLE);
        VkPipelineBinaryHandlesInfoKHR handles{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR,
            .pipelineBinaryCount = count,
            .pPipelineBinaries = binaries.data(),
        };
        VkResult res = dld->vkCreatePipelineBinariesKHR(device, &binary_ci, nullptr, &handles);
        if (res == VK_SUCCESS) {
            const VkPipelineBinaryInfoKHR binary_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR,
                .pNext = ci.pNext,
                .binaryCount = count,
                .pPipelineBinaries = binaries.data(),
            };
            CreateInfo from_binaries = ci;
            from_binaries.pNext = &binary_info;
            // A pipeline built from binaries takes no pipeline cache.
            res = create(from_binaries, VK_NULL_HANDLE, out_pipeline);
        }
        for (const VkPipelineBinaryKHR binary : binaries) {
            if (binary != VK_NULL_HANDLE) {
                dld->vkDestroyPipelineBinaryKHR(device, binary, nullptr);
            }
        }
        if (res != VK_SUCCESS) {
            if (*out_pipeline != VK_NULL_HANDLE) {
                dld->vkDestroyPipeline(device, *out_pipeline, nullptr);
            }
            *out_pipeline = VK_NULL_HANDLE;
            if (res == VK_ERROR_DEVICE_LOST) {
                vk::Check(res);
            }
            return false;
        }
        return true;
    }

    void Save(const std::string& key, VkPipeline pipeline) {
        const VkPipelineBinaryCreateInfoKHR binary_ci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR,
            .pipeline = pipeline,
        };
        VkPipelineBinaryHandlesInfoKHR handles{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR,
        };
        if (dld->vkCreatePipelineBinariesKHR(device, &binary_ci, nullptr, &handles) !=
                VK_SUCCESS ||
            handles.pipelineBinaryCount == 0 ||
            handles.pipelineBinaryCount > MAX_BINARIES_PER_PIPELINE) {
            return;
        }
        std::vector<VkPipelineBinaryKHR> binaries(handles.pipelineBinaryCount, VK_NULL_HANDLE);
        SCOPE_EXIT {
            for (const auto binary : binaries) {
                if (binary != VK_NULL_HANDLE) {
                    dld->vkDestroyPipelineBinaryKHR(device, binary, nullptr);
                }
            }
        };
        handles.pPipelineBinaries = binaries.data();
        if (dld->vkCreatePipelineBinariesKHR(device, &binary_ci, nullptr, &handles) !=
                VK_SUCCESS ||
            handles.pipelineBinaryCount != binaries.size()) {
            return;
        }
        std::vector<Binary> captured;
        captured.reserve(binaries.size());
        u64 total_bytes = 0;
        u64 queued_bytes = RECORD_HEADER_BYTES + binaries.size() * PACKED_KEY_BYTES;
        for (const VkPipelineBinaryKHR binary : binaries) {
            const VkPipelineBinaryDataInfoKHR data_info{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_DATA_INFO_KHR,
                .pipelineBinary = binary,
            };
            VkPipelineBinaryKeyKHR binary_key{.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
            size_t size = 0;
            if (dld->vkGetPipelineBinaryDataKHR(device, &data_info, &binary_key, &size, nullptr) !=
                    VK_SUCCESS ||
                size == 0 || size > MAX_PAYLOAD_BYTES - total_bytes ||
                binary_key.keySize == 0 || binary_key.keySize > KEY_BYTES) {
                return;
            }
            total_bytes += size;
            Binary data{std::string(reinterpret_cast<const char*>(binary_key.key),
                                    binary_key.keySize), {}};
            bool stored;
            {
                std::scoped_lock lock{mutex};
                stored = blobs.contains(data.key);
            }
            if (!stored) {
                data.data.resize(size);
                if (dld->vkGetPipelineBinaryDataKHR(device, &data_info, &binary_key, &size,
                                                    data.data.data()) != VK_SUCCESS ||
                    size != data.data.size() || binary_key.keySize != data.key.size() ||
                    std::memcmp(binary_key.key, data.key.data(), data.key.size()) != 0) {
                    return;
                }
                queued_bytes += RECORD_HEADER_BYTES + size;
            }
            captured.push_back(std::move(data));
        }
        {
            std::scoped_lock lock{mutex};
            if (index.contains(key) || !out.is_open() ||
                pending_bytes + queued_bytes > MAX_PENDING_BYTES) {
                return;
            }
            pending_bytes += queued_bytes;
        }
        writer.QueueWork([this, key, captured = std::move(captured), queued_bytes] {
            std::scoped_lock lock{mutex};
            pending_bytes -= queued_bytes;
            if (!index.contains(key) && out.is_open()) {
                Append(key, captured);
            }
        });
    }

    bool AppendRecord(u32 type, RecordIndex& records, const std::string& key,
                      const std::vector<u8>& payload) {
        const u64 checksum = PayloadHash(payload.data(), payload.size());
        WriteRecord(out, type, key, payload, checksum);
        if (!out) {
            LOG_ERROR(Render_Vulkan, "Pipeline binaries: can't write {}, saving stopped",
                      path.string());
            out.close();
            return false;
        }
        Remember(records, key, {file_end + RECORD_HEADER_BYTES, payload.size(), checksum, true});
        file_end += RECORD_HEADER_BYTES + payload.size();
        return true;
    }

    void Append(const std::string& key, const std::vector<Binary>& captured) {
        std::vector<u8> keys(captured.size() * PACKED_KEY_BYTES);
        std::unordered_set<std::string_view> new_keys;
        u64 needed = RECORD_HEADER_BYTES + keys.size();
        for (size_t i = 0; i < captured.size(); ++i) {
            const auto& binary = captured[i];
            const u32 size = static_cast<u32>(binary.key.size());
            auto* packed = keys.data() + i * PACKED_KEY_BYTES;
            std::memcpy(packed, &size, sizeof(size));
            std::memcpy(packed + sizeof(size), binary.key.data(), size);
            if (!blobs.contains(binary.key) && new_keys.insert(binary.key).second) {
                if (binary.data.empty()) {
                    return;
                }
                needed += RECORD_HEADER_BYTES + binary.data.size();
            }
        }
        if (needed > MAX_FILE_BYTES - file_end) {
            return;
        }
        for (const auto& binary : captured) {
            if (!blobs.contains(binary.key) &&
                !AppendRecord(BINARY_RECORD, blobs, binary.key, binary.data)) {
                return;
            }
        }
        if (AppendRecord(PIPELINE_RECORD, index, key, keys)) {
            out.flush();
            if (out) {
                ++stats.saved;
            } else {
                LOG_ERROR(Render_Vulkan, "Pipeline binaries: can't flush {}, saving stopped",
                          path.string());
                out.close();
            }
        }
    }

    static u64 PayloadHash(const u8* data, size_t size) {
        return Common::CityHash64(reinterpret_cast<const char*>(data), size);
    }

    static void WriteRecord(std::ofstream& file, u32 type, const std::string& key,
                            const std::vector<u8>& data, u64 checksum) {
        const u64 size = data.size();
        const u32 key_size = static_cast<u32>(key.size());
        std::array<u8, KEY_BYTES> key_padded{};
        std::memcpy(key_padded.data(), key.data(), key.size());
        file.write(reinterpret_cast<const char*>(&type), 4);
        file.write(reinterpret_cast<const char*>(&key_size), 4);
        file.write(reinterpret_cast<const char*>(key_padded.data()), KEY_BYTES);
        file.write(reinterpret_cast<const char*>(&size), 8);
        file.write(reinterpret_cast<const char*>(&checksum), 8);
        file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size));
    }

    // Called with the mutex held and the append stream closed.
    void Compact() {
        const bool too_large = file_end >= MAX_FILE_BYTES - MAX_PAYLOAD_BYTES;
        if (!too_large && (dead_bytes < MIN_DEAD_BYTES || dead_bytes * 4 < file_end)) {
            return;
        }
        std::vector<std::pair<const std::string*, const Record*>> keep;
        keep.reserve(index.size());
        for (const auto& [key, record] : index) {
            keep.emplace_back(&key, &record);
        }
        // Used this session first, then newest first; the file keeps its record order.
        std::ranges::sort(keep, [](const auto& a, const auto& b) {
            return a.second->used != b.second->used ? a.second->used
                                                    : a.second->offset > b.second->offset;
        });
        u64 kept_bytes = HEADER_BYTES;
        const u64 budget = too_large ? MAX_FILE_BYTES / 4 * 3 : ~0ULL;
        std::unordered_set<std::string_view> kept_blob_keys;
        std::vector<RecordIndex::const_iterator> kept_blobs;
        std::vector<u8> payload;
        std::erase_if(keep, [&](const auto& entry) {
            if (!ReadRecord(*entry.second, payload)) {
                return true;
            }
            const auto keys = UnpackKeys(payload);
            if (keys.empty()) {
                return true;
            }
            u64 size = RECORD_HEADER_BYTES + entry.second->size;
            std::vector<RecordIndex::const_iterator> extra_blobs;
            for (const auto& key : keys) {
                const RecordIndex::const_iterator blob = blobs.find(key);
                if (blob == blobs.end()) {
                    return true;
                }
                if (!kept_blob_keys.contains(key) &&
                    std::ranges::find(extra_blobs, blob) == extra_blobs.end()) {
                    extra_blobs.push_back(blob);
                    size += RECORD_HEADER_BYTES + blob->second.size;
                }
            }
            if (size > budget - kept_bytes) {
                return true;
            }
            kept_bytes += size;
            for (const auto& blob : extra_blobs) {
                kept_blob_keys.insert(blob->first);
                kept_blobs.push_back(blob);
            }
            return false;
        });
        std::ranges::sort(kept_blobs, {}, [](const auto& blob) { return blob->second.offset; });
        std::ranges::sort(keep, {}, [](const auto& entry) { return entry.second->offset; });

        auto temp = path;
        temp += ".tmp";
        bool ok;
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(header.data()), header.size());
            auto copy = [&](u32 type, const std::string& key, const Record& record) {
                if (!ReadRecord(record, payload)) {
                    return false;
                }
                WriteRecord(file, type, key, payload, record.checksum);
                return file.good();
            };
            ok = std::ranges::all_of(kept_blobs, [&](const auto& blob) {
                return copy(BINARY_RECORD, blob->first, blob->second);
            }) && std::ranges::all_of(keep, [&](const auto& entry) {
                return copy(PIPELINE_RECORD, *entry.first, *entry.second);
            });
            file.close();
            ok = ok && file.good();
        }
        in.close();
        std::error_code ec;
        if (ok) {
            std::filesystem::rename(temp, path, ec);
        }
        if (!ok || ec) {
            std::filesystem::remove(temp, ec);
            LOG_WARNING(Render_Vulkan, "Pipeline binaries: can't rewrite {}, keeping it",
                        path.string());
            return;
        }
        LOG_INFO(Render_Vulkan,
                 "Pipeline binaries: rewrote {} with {} of {} pipelines ({:.1f} of {:.1f} MiB)",
                 path.string(), keep.size(), index.size(), Mib(kept_bytes), Mib(file_end));
    }

    VkDevice device{};
    const vk::DeviceDispatch* dld{};
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file_lock{nullptr, &std::fclose};
    std::filesystem::path path;
    std::array<u8, HEADER_BYTES> header{};
    std::ofstream out;
    std::ifstream in;
    u64 file_end{};
    u64 dead_bytes{};
    u64 pending_bytes{};
    RecordIndex index;
    RecordIndex blobs;
    u64 recent_blob_offset{};
    BinaryData recent_blob;
    std::mutex mutex;
    Stats stats;
    Common::ThreadWorker writer{1, "VkBinarySave", {}, Common::ThreadPlacement::Background};
};

std::shared_mutex active_mutex;
std::unique_ptr<Store> active;

} // Anonymous namespace

bool Activate(VkDevice device, const vk::DeviceDispatch& dld, const std::filesystem::path& file) {
    if (!dld.vkGetPipelineKeyKHR || !dld.vkCreatePipelineBinariesKHR ||
        !dld.vkDestroyPipelineBinaryKHR || !dld.vkGetPipelineBinaryDataKHR ||
        !dld.vkReleaseCapturedPipelineDataKHR) {
        return false;
    }
    std::unique_lock lock{active_mutex};
    if (active) {
        return false;
    }
    auto store = std::make_unique<Store>();
    if (!store->Open(device, dld, file)) {
        return false;
    }
    active = std::move(store);
    return true;
}

void Deactivate(VkDevice device) {
    std::unique_lock lock{active_mutex};
    if (active && active->Owns(device)) {
        active->Close();
        active.reset();
    }
}

bool CreateGraphicsPipeline(VkDevice device, const vk::DeviceDispatch& dld,
                            const VkGraphicsPipelineCreateInfo& ci, VkPipelineCache cache,
                            VkPipeline* out, VkResult* result) {
    if (cache != VK_NULL_HANDLE || GetPersistenceMode() != PersistenceMode::Auto) {
        return false;
    }
    std::shared_lock lock{active_mutex};
    if (!active || !active->Owns(device)) {
        return false;
    }
    return active->Create(ci, out, result,
                          [&](const VkGraphicsPipelineCreateInfo& info, VkPipelineCache c,
                              VkPipeline* p) {
                              return dld.vkCreateGraphicsPipelines(device, c, 1, &info, nullptr,
                                                                   p);
                          });
}

bool CreateComputePipeline(VkDevice device, const vk::DeviceDispatch& dld,
                           const VkComputePipelineCreateInfo& ci, VkPipelineCache cache,
                           VkPipeline* out, VkResult* result) {
    if (cache != VK_NULL_HANDLE || GetPersistenceMode() != PersistenceMode::Auto) {
        return false;
    }
    std::shared_lock lock{active_mutex};
    if (!active || !active->Owns(device)) {
        return false;
    }
    return active->Create(ci, out, result,
                          [&](const VkComputePipelineCreateInfo& info, VkPipelineCache c,
                              VkPipeline* p) {
                              return dld.vkCreateComputePipelines(device, c, 1, &info, nullptr,
                                                                  p);
                          });
}

} // namespace Vulkan::PipelineBinaryCache
