// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <openssl/evp.h>
#include <zstd.h>

#include "core/file_sys/content_archive.h"
#include "core/file_sys/nup_container.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"

namespace {

constexpr std::size_t TocOffset = 0x80;
constexpr std::size_t PackageOffset = TocOffset + 0x30;
constexpr std::size_t DirOffset = PackageOffset + 0x40;

void Put(std::vector<u8>& bytes, std::size_t offset, u64 value, std::size_t width = 8) {
    for (std::size_t i = 0; i < width; ++i) {
        bytes[offset + i] = static_cast<u8>(value >> (8 * i));
    }
}

struct NupMember {
    std::string_view name;
    std::vector<u8> extent;
    u64 size;
    u8 codec{};
};

std::vector<u8> Compress(const std::vector<u8>& bytes) {
    std::vector<u8> result(ZSTD_compressBound(bytes.size()));
    const auto size = ZSTD_compress(result.data(), result.size(), bytes.data(), bytes.size(), 1);
    REQUIRE_FALSE(ZSTD_isError(size));
    result.resize(size);
    return result;
}

struct NupFixture {
    explicit NupFixture(const std::vector<std::vector<u32>>& dirs = {{1}},
                        std::string_view filename = {}, u32 package_count = 1)
        : NupFixture(dirs, std::vector<NupMember>{{filename, {0x42}, 1}}, package_count) {}

    NupFixture(const std::vector<std::vector<u32>>& dirs,
               const std::vector<NupMember>& members, u32 package_count = 1) {
        const std::size_t dir_offset = PackageOffset + package_count * 0x40;
        file_offset = dir_offset + dirs.size() * 0x18;
        const std::size_t children_offset = file_offset + members.size() * 0x50;
        std::size_t child_count{};
        for (const auto& children : dirs) {
            child_count += children.size();
        }
        const std::size_t pool_offset = children_offset + child_count * sizeof(u32);
        std::size_t pool_size{};
        for (const auto& member : members) {
            pool_size += member.name.size();
        }
        toc_size = pool_offset + pool_size - TocOffset;
        const std::size_t blob_offset = (TocOffset + toc_size + 15) & ~std::size_t{15};
        bytes.resize(blob_offset);
        std::copy(FileSys::NupContainer::HeaderMagic.begin(),
                  FileSys::NupContainer::HeaderMagic.end(), bytes.begin());
        Put(bytes, 0x04, 0x80, 2);
        Put(bytes, 0x08, TocOffset);
        Put(bytes, 0x10, toc_size);
        Put(bytes, 0x38, blob_offset);
        Put(bytes, 0x48, package_count, 4);
        Put(bytes, TocOffset, 0x01434F54, 4);
        Put(bytes, TocOffset + 0x04, package_count, 4);
        Put(bytes, TocOffset + 0x08, dirs.size());
        Put(bytes, TocOffset + 0x10, members.size());
        Put(bytes, TocOffset + 0x18, pool_offset - TocOffset);
        Put(bytes, TocOffset + 0x20, pool_size);
        std::size_t slot{};
        for (std::size_t i = 0; i < dirs.size(); ++i) {
            Put(bytes, dir_offset + i * 0x18 + 0x08, slot, 4);
            Put(bytes, dir_offset + i * 0x18 + 0x0C, dirs[i].size(), 4);
            for (u32 child : dirs[i]) {
                Put(bytes, children_offset + slot++ * sizeof(u32), child, 4);
            }
        }
        std::size_t name_offset{};
        for (std::size_t i = 0; i < members.size(); ++i) {
            const auto& member = members[i];
            const auto record = file_offset + i * 0x50;
            const auto extent_offset = (bytes.size() + 15) & ~std::size_t{15};
            bytes.resize(extent_offset);
            bytes.insert(bytes.end(), member.extent.begin(), member.extent.end());
            Put(bytes, record, name_offset, 4);
            Put(bytes, record + 0x04, member.name.size(), 4);
            Put(bytes, record + 0x08, member.size);
            Put(bytes, record + 0x10, extent_offset);
            Put(bytes, record + 0x18, member.extent.size());
            Put(bytes, record + 0x40, member.codec, 1);
            std::copy(member.name.begin(), member.name.end(),
                      bytes.begin() + pool_offset + name_offset);
            name_offset += member.name.size();
        }
        bytes.resize((bytes.size() + 15) & ~std::size_t{15});
        Put(bytes, 0x40, bytes.size());
        UpdateHash();
    }

    void UpdateHash() {
        std::array<u8, 32> hash{};
        unsigned int length{};
        REQUIRE(EVP_Digest(bytes.data() + TocOffset, toc_size, hash.data(), &length, EVP_sha256(),
                           nullptr) == 1);
        REQUIRE(length == hash.size());
        std::copy(hash.begin(), hash.end(), bytes.begin() + 0x18);
    }

    FileSys::NupContainer Parse() const {
        return FileSys::NupContainer(std::make_shared<FileSys::VectorVfsFile>(bytes));
    }

    std::vector<u8> bytes;
    std::size_t file_offset{};
    std::size_t toc_size{};
};

} // namespace

TEST_CASE("NUP serves texture stand-ins only to the game", "[nup][nup-validation]") {
    constexpr auto damaged_name = "Rock_WaterTemple_Gate_Nrm.bntx.zs";
    constexpr auto good_name = "Rock_WaterTemple_Floor_Nrm.bntx.zs";
    constexpr auto unknown_name = "Rock_WaterTemple_Unknown_Nrm.bntx.zs";
    constexpr auto healthy_name = "Soil_FairyFountain_Floor_Nrm.bntx.zs";
    constexpr auto unmatched_name = "Rock_WaterTemple_Gate_Alb.bntx.zs";
    constexpr auto unrelated_name = "Layout_Title_Font.zs";
    const std::vector<u8> damaged(12 * 1024, 0);
    const auto damaged_extent = Compress(damaged);
    const auto good = Compress({0x01});
    const auto other = Compress({0x02});
    const auto healthy = Compress({0x03});
    const std::vector<NupMember> members{
        {damaged_name, damaged_extent, damaged.size(), 1},
        {"Rock_Cave_Nrm.bntx.zs", other, other.size()},
        {good_name, good, good.size()},
        {unknown_name, {1, 2, 3, 4}, damaged.size(), 1},
        {healthy_name, Compress(healthy), healthy.size(), 1},
        {unmatched_name, damaged_extent, damaged.size(), 1},
        {unrelated_name, damaged_extent, damaged.size(), 1},
        {"Layout_Game_Font.zs", good, good.size()},
    };
    auto container = NupFixture({{1, 2, 3, 4, 5, 6, 7, 8}}, members).Parse();
    REQUIRE(container.GetStatus() == Loader::ResultStatus::Success);
    const auto romfs = container.GetRomFSDir(0);
    REQUIRE(romfs);
    REQUIRE(romfs->GetFiles().size() == members.size());
    const auto substitute = romfs->GetFile(damaged_name);
    REQUIRE(substitute);
    CHECK(substitute->GetName() == damaged_name);
    CHECK(substitute->ReadAllBytes() == good);
    CHECK(romfs->GetFile(good_name)->ReadAllBytes() == good);
    CHECK(romfs->GetFile(healthy_name)->ReadAllBytes() == healthy);
    CHECK(romfs->GetFile(unmatched_name)->ReadAllBytes() == damaged);
    CHECK(romfs->GetFile(unrelated_name)->ReadAllBytes() == damaged);
    CHECK(romfs->GetFile(unknown_name)->ReadAllBytes().empty());
    CHECK(container.GetPackageDir(0)->GetFile(damaged_name)->ReadAllBytes() == damaged);
    CHECK(container.GetExeFSDir(0)->GetFile(damaged_name)->ReadAllBytes() == damaged);
}

TEST_CASE("NUP rejects wrapping table counts", "[nup][nup-validation]") {
    NupFixture fixture;
    SECTION("directory records") {
        Put(fixture.bytes, TocOffset + 0x08, (u64{1} << 61) + 1);
    }
    SECTION("file records") {
        Put(fixture.bytes, TocOffset + 0x10, (u64{1} << 60) + 1);
    }
    fixture.UpdateHash();
    REQUIRE(fixture.Parse().GetStatus() == Loader::ResultStatus::ErrorNUPBadFormat);
}

TEST_CASE("NUP rejects wrapping ranges", "[nup][nup-validation]") {
    NupFixture fixture;
    SECTION("TOC size") {
        Put(fixture.bytes, 0x10, std::numeric_limits<u64>::max());
    }
    SECTION("TOC offset") {
        Put(fixture.bytes, 0x08, std::numeric_limits<u64>::max() - 15);
    }
    SECTION("string pool") {
        Put(fixture.bytes, TocOffset + 0x20,
            std::numeric_limits<u64>::max() - fixture.toc_size + 1);
    }
    SECTION("directory children") {
        Put(fixture.bytes, DirOffset + 0x08, std::numeric_limits<u32>::max(), 4);
        Put(fixture.bytes, DirOffset + 0x0C, 1, 4);
    }
    SECTION("directory name") {
        Put(fixture.bytes, DirOffset, std::numeric_limits<u32>::max(), 4);
        Put(fixture.bytes, DirOffset + 0x04, 1, 4);
    }
    SECTION("file name") {
        Put(fixture.bytes, fixture.file_offset, std::numeric_limits<u32>::max(), 4);
        Put(fixture.bytes, fixture.file_offset + 0x04, 1, 4);
    }
    SECTION("file extent") {
        Put(fixture.bytes, fixture.file_offset + 0x10, std::numeric_limits<u64>::max() - 15);
        Put(fixture.bytes, fixture.file_offset + 0x18, 32);
    }
    fixture.UpdateHash();
    REQUIRE(fixture.Parse().GetStatus() == Loader::ResultStatus::ErrorNUPBadFormat);
}

TEST_CASE("NUP ignores optional metadata with wrapping bounds", "[nup][nup-validation]") {
    NupFixture fixture;
    Put(fixture.bytes, PackageOffset + 0x20, std::numeric_limits<u64>::max() - 15);
    Put(fixture.bytes, PackageOffset + 0x28, 32, 4);
    fixture.UpdateHash();
    auto container = fixture.Parse();
    REQUIRE(container.GetStatus() == Loader::ResultStatus::Success);
    REQUIRE(container.GetPackages().size() == 1);
    CHECK(container.GetPackages()[0].meta_offset == 0);
    CHECK(container.GetPackages()[0].meta_size == 0);
}

TEST_CASE("NUP rejects directory cycles", "[nup][nup-validation]") {
    SECTION("self reference") {
        REQUIRE(NupFixture({{0}}).Parse().GetStatus() == Loader::ResultStatus::ErrorNUPBadFormat);
    }
    SECTION("ancestor reference") {
        REQUIRE(NupFixture({{1}, {0}}).Parse().GetStatus() ==
                Loader::ResultStatus::ErrorNUPBadFormat);
    }
    SECTION("unreachable cycle") {
        REQUIRE(NupFixture({{}, {2}, {1}}).Parse().GetStatus() ==
                Loader::ResultStatus::ErrorNUPBadFormat);
    }
}

TEST_CASE("NUP bounds directory depth while allowing shared subtrees", "[nup][nup-validation]") {
    SECTION("depth limit") {
        std::vector<std::vector<u32>> dirs(128);
        for (u32 i = 0; i < dirs.size(); ++i) {
            dirs[i].push_back(i + 1);
        }
        auto container = NupFixture(dirs).Parse();
        REQUIRE(container.GetStatus() == Loader::ResultStatus::Success);
        auto dir = container.GetExeFSDir(0);
        for (u32 i = 1; i < dirs.size(); ++i) {
            REQUIRE(dir->GetSubdirectories().size() == 1);
            dir = dir->GetSubdirectories()[0];
        }
        REQUIRE(dir->GetFiles().size() == 1);
        CHECK(dir->GetFiles()[0]->ReadByte(0) == 0x42);
        dirs.push_back({129});
        REQUIRE(NupFixture(dirs).Parse().GetStatus() == Loader::ResultStatus::ErrorNUPBadFormat);
    }
    SECTION("shared subtree") {
        auto container = NupFixture({{1, 2}, {3}, {3}, {4}}).Parse();
        REQUIRE(container.GetStatus() == Loader::ResultStatus::Success);
        REQUIRE(container.GetExeFSDir(0)->GetSubdirectories().size() == 2);
    }
    SECTION("previously visited subtree exceeds depth limit") {
        std::vector<std::vector<u32>> dirs(129);
        for (u32 i = 1; i < dirs.size(); ++i) {
            dirs[i].push_back(i - 1);
        }
        REQUIRE(NupFixture(dirs).Parse().GetStatus() == Loader::ResultStatus::ErrorNUPBadFormat);
    }
}

TEST_CASE("NUP exposes the newest matching add-on as readable content", "[nup][nup-content]") {
    constexpr u64 base = 0x0100123456780000;
    constexpr u64 addon = base + 0x1001;
    NupFixture fixture({{2}, {}}, "dlc.bin", 3);
    const auto write_addon = [&](std::size_t index, u64 title_id, u32 version, u32 directory) {
        const auto offset = PackageOffset + index * 0x40;
        Put(fixture.bytes, offset, title_id);
        Put(fixture.bytes, offset + 0x0C, std::numeric_limits<u32>::max(), 4);
        Put(fixture.bytes, offset + 0x10, directory, 4);
        Put(fixture.bytes, offset + 0x14, version, 4);
        Put(fixture.bytes, offset + 0x18, static_cast<u8>(FileSys::NupContainer::PackageKind::Addon), 1);
    };
    write_addon(0, addon, 2, 0);
    write_addon(1, addon, 1, 1);
    write_addon(2, addon + 0x2000, 1, 1);
    fixture.UpdateHash();

    FileSys::ManualContentProvider provider;
    REQUIRE(provider.AddEntriesFromContainer(
        std::make_shared<FileSys::VectorVfsFile>(std::move(fixture.bytes), "test.nup"), true, base));
    const auto entries =
        provider.ListEntriesFilter(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data, {});
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().title_id == addon);
    const auto nca = provider.GetEntry(addon, FileSys::ContentRecordType::Data);
    REQUIRE(nca != nullptr);
    REQUIRE(nca->GetStatus() == Loader::ResultStatus::Success);
    const auto romfs = FileSys::ExtractRomFS(nca->GetRomFS());
    REQUIRE(romfs != nullptr);
    const auto member = romfs->GetFile("dlc.bin");
    REQUIRE(member != nullptr);
    CHECK(member->ReadByte(0) == 0x42);
}
