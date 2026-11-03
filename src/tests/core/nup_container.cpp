// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <cstring>
#include <limits>
#include <catch2/catch_test_macros.hpp>
#include <openssl/evp.h>
#include <zstd.h>
#include "core/file_sys/nup_container.h"
#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"

namespace {
using Bytes = std::vector<u8>;
void Put(Bytes& b, std::size_t offset, u64 value, unsigned width) {
    REQUIRE(offset + width <= b.size());
    for (unsigned i = 0; i < width; ++i)
        b[offset + i] = static_cast<u8>(value >> (8 * i));
}
u64 Get(const Bytes& b, std::size_t offset, unsigned width) {
    u64 value{};
    for (unsigned i = 0; i < width; ++i)
        value |= u64(b[offset + i]) << (8 * i);
    return value;
}
void Hash(const u8* data, std::size_t size, u8* out) {
    unsigned int count{};
    REQUIRE(EVP_Digest(data, size, out, &count, EVP_sha256(), nullptr) == 1);
    REQUIRE(count == 32);
}
void Rehash(Bytes& b) {
    Hash(b.data() + 128, Get(b, 16, 8), b.data() + 24);
}
Bytes Fixture(bool compressed = false, std::size_t length = 4096) {
    // root -> romfs -> payload.bin. No copyrighted game data or console keys.
    const std::string pool = std::string("\0romfs\0payload.bin\0", 19);
    const std::size_t dirs = 48 + 64, files = dirs + 48, children = files + 80,
                      pool_off = children + 8;
    const std::size_t toc_size = (pool_off + pool.size() + 15) & ~std::size_t(15);
    Bytes payload(length, 0x71), stored = payload;
    if (compressed) {
        stored.resize(ZSTD_compressBound(payload.size()));
        const auto size =
            ZSTD_compress(stored.data(), stored.size(), payload.data(), payload.size(), 3);
        REQUIRE_FALSE(ZSTD_isError(size));
        stored.resize(size);
    }
    const auto blob = 128 + toc_size;
    Bytes b(blob + stored.size());
    std::memcpy(b.data(), "NUP\1", 4);
    Put(b, 4, 128, 2);
    Put(b, 6, 1, 2);
    Put(b, 8, 128, 8);
    Put(b, 16, toc_size, 8);
    Put(b, 56, blob, 8);
    Put(b, 64, b.size(), 8);
    Put(b, 72, 1, 4);
    const std::size_t t = 128;
    std::memcpy(b.data() + t, "TOC\1", 4);
    Put(b, t + 4, 1, 4);
    Put(b, t + 8, 2, 8);
    Put(b, t + 16, 1, 8);
    Put(b, t + 24, pool_off, 8);
    Put(b, t + 32, pool.size(), 8);
    Put(b, t + 48, 0x040074A01BF12000ull, 8);
    Put(b, t + 48 + 12, 0xffffffff, 4);
    Put(b, t + 48 + 16, 1, 4);
    Put(b, t + dirs + 8, 0, 4);
    Put(b, t + dirs + 12, 1, 4);
    Put(b, t + dirs + 24, 1, 4);
    Put(b, t + dirs + 28, 5, 4);
    Put(b, t + dirs + 32, 1, 4);
    Put(b, t + dirs + 36, 1, 4);
    Put(b, t + files, 7, 4);
    Put(b, t + files + 4, 11, 4);
    Put(b, t + files + 8, length, 8);
    Put(b, t + files + 16, length ? blob : 0, 8);
    Put(b, t + files + 24, stored.size(), 8);
    Hash(payload.data(), payload.size(), b.data() + t + files + 32);
    b[t + files + 64] = compressed ? 1 : 0;
    Put(b, t + children, 1, 4);
    Put(b, t + children + 4, 2, 4);
    std::memcpy(b.data() + t + pool_off, pool.data(), pool.size());
    std::copy(stored.begin(), stored.end(), b.begin() + blob);
    Rehash(b);
    return b;
}
FileSys::NupContainer Mount(Bytes b) {
    return FileSys::NupContainer(
        std::make_shared<FileSys::VectorVfsFile>(std::move(b), "test.nup"));
}
} // namespace

TEST_CASE("NUP raw and zstd support random reads and integrity", "[nup]") {
    for (bool compressed : {false, true}) {
        auto nup = Mount(Fixture(compressed));
        REQUIRE(nup.GetStatus() == Loader::ResultStatus::Success);
        REQUIRE(nup.FindBasePackage() == 0);
        auto file = nup.GetRomFSDir(0)->GetFile("payload.bin");
        REQUIRE(file);
        REQUIRE(file->ReadBytes(31, 77) == Bytes(31, 0x71));
        REQUIRE(file->ReadBytes(20, 4090) == Bytes(6, 0x71));
        REQUIRE(nup.VerifyIntegrity({}) == Loader::ResultStatus::Success);
        REQUIRE(nup.VerifyIntegrity([](auto, auto) { return false; }) ==
                Loader::ResultStatus::ErrorIntegrityVerificationFailed);
    }
}

TEST_CASE("NUP handles empty members and streams large compressed members", "[nup]") {
    auto empty = Mount(Fixture(false, 0));
    REQUIRE(empty.GetStatus() == Loader::ResultStatus::Success);
    REQUIRE(empty.GetRomFSDir(0)->GetFile("payload.bin")->GetSize() == 0);
    REQUIRE(empty.VerifyIntegrity({}) == Loader::ResultStatus::Success);
    const auto size = FileSys::NupDecompressCache::MaxCachedExtent + 1024;
    auto large = Mount(Fixture(true, size));
    REQUIRE(large.GetStatus() == Loader::ResultStatus::Success);
    auto file = large.GetRomFSDir(0)->GetFile("payload.bin");
    REQUIRE(file);
    REQUIRE(file->ReadBytes(19, size - 19) == Bytes(19, 0x71));
    REQUIRE(file->ReadBytes(113, 1000000) == Bytes(113, 0x71));
}

TEST_CASE("NUP rejects corrupt ranges counts names and directory graphs", "[nup]") {
    constexpr std::size_t t = 128, dirs = 112, files = 160, children = 240;
    for (int mutation = 0; mutation < 13; ++mutation) {
        auto b = Fixture();
        switch (mutation) {
        case 0:
            Put(b, 8, std::numeric_limits<u64>::max() - 3, 8);
            break;
        case 1:
            Put(b, 16, std::numeric_limits<u64>::max(), 8);
            break;
        case 2:
            Put(b, t + 8, std::numeric_limits<u64>::max(), 8);
            break;
        case 3:
            Put(b, t + 16, std::numeric_limits<u64>::max(), 8);
            break;
        case 4:
            Put(b, t + 24, std::numeric_limits<u64>::max(), 8);
            break;
        case 5:
            Put(b, t + dirs + 8, 0xffffffff, 4);
            break;
        case 6:
            Put(b, t + dirs + 12, 0xffffffff, 4);
            break;
        case 7:
            Put(b, t + children + 4, 1, 4);
            break;
        case 8:
            Put(b, t + files + 24, 4095, 8);
            break;
        case 9:
            Put(b, t + files + 16, std::numeric_limits<u64>::max() - 3, 8);
            break;
        case 10:
            b[t + files + 64] = 2;
            break;
        case 11:
            Put(b, t + files + 4, 0xffffffff, 4);
            break;
        case 12:
            b[Get(b, 8, 8) + Get(b, t + 24, 8) + 7] = '/';
            break;
        }
        if (mutation >= 2)
            Rehash(b);
        auto nup = Mount(std::move(b));
        INFO(mutation);
        REQUIRE(nup.GetStatus() != Loader::ResultStatus::Success);
    }
    auto changed = Fixture();
    changed[150] ^= 1;
    REQUIRE(Mount(std::move(changed)).GetStatus() != Loader::ResultStatus::Success);
    auto short_file = Fixture();
    short_file.pop_back();
    REQUIRE(Mount(std::move(short_file)).GetStatus() != Loader::ResultStatus::Success);
    auto payload = Fixture();
    payload.back() ^= 1;
    auto nup = Mount(std::move(payload));
    REQUIRE(nup.GetStatus() == Loader::ResultStatus::Success);
    REQUIRE(nup.VerifyIntegrity({}) == Loader::ResultStatus::ErrorIntegrityVerificationFailed);
}

TEST_CASE("NUP update overlay shadows both files and directories", "[nup]") {
    using namespace FileSys;
    auto base = std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{std::make_shared<VectorVfsFile>(Bytes{1}, "same")},
        std::vector<VirtualDir>{std::make_shared<VectorVfsDirectory>(
            std::vector<VirtualFile>{}, std::vector<VirtualDir>{}, "old-dir")});
    auto update = std::make_shared<VectorVfsDirectory>(
        std::vector<VirtualFile>{std::make_shared<VectorVfsFile>(Bytes{2}, "old-dir")},
        std::vector<VirtualDir>{std::make_shared<VectorVfsDirectory>(
            std::vector<VirtualFile>{}, std::vector<VirtualDir>{}, "same")});
    auto merged = NupContainer::MergeShadowDirs(base, update);
    REQUIRE(merged->GetFile("same") == nullptr);
    REQUIRE(merged->GetSubdirectory("same") != nullptr);
    REQUIRE(merged->GetSubdirectory("old-dir") == nullptr);
    REQUIRE(merged->GetFile("old-dir")->ReadAllBytes() == Bytes{2});
}
