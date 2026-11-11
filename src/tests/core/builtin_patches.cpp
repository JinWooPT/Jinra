// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include "common/common_funcs.h"
#include "common/fs/path_util.h"
#include "common/hex_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/nso.h"

namespace {

constexpr u64 OcarinaTitleId = 0x0400486024DEC000;
constexpr u64 OtherTitleId = 0x0100000000010000;
constexpr std::string_view MaxResolution = "Max Resolution (built-in)";
constexpr std::string_view DynamicFps = "Dynamic FPS by Dark (built-in)";
constexpr std::string_view OcarinaBuildIdHex = "E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348";
constexpr FileSys::PatchManager::BuildID OcarinaBuildId{
    0xE0, 0xF3, 0xCC, 0xFA, 0xF5, 0x00, 0x65, 0xAE, 0x4E, 0xA0,
    0x1E, 0xB5, 0xAE, 0xB1, 0x3B, 0x1A, 0x45, 0xDA, 0x33, 0x48,
};
constexpr size_t PatchOffset = sizeof(Loader::NSOHeader) + 0x805BD0;
constexpr u32 TbzInstruction = 0x360041E8;
constexpr u32 BranchInstruction = 0x1400020F;
constexpr u32 NopInstruction = 0xD503201F;
constexpr size_t DynamicCallOffset = sizeof(Loader::NSOHeader) + 0x6E6CC4;
constexpr size_t DynamicRoutineOffset = sizeof(Loader::NSOHeader) + 0x3C46200;
constexpr size_t DynamicStateOffset = sizeof(Loader::NSOHeader) + 0x4E9FE00;
constexpr size_t DynamicRoutineSize = 120;
constexpr u32 DynamicCallInstruction = 0x94D57D4F;
constexpr u32 DynamicOriginalInstruction = 0x1E201828;

constexpr std::array<std::pair<Common::FS::EdenPath, const char*>, 4> TestDirectories{{
    {Common::FS::EdenPath::NANDDir, "nand"},
    {Common::FS::EdenPath::SDMCDir, "sdmc"},
    {Common::FS::EdenPath::LoadDir, "load"},
    {Common::FS::EdenPath::DumpDir, "dump"},
}};

void WriteIps(const std::filesystem::path& path, std::span<const u8> bytes, size_t offset) {
    const bool ips32 = offset > 0xFFFFFF;
    std::vector<u8> ips =
        ips32 ? std::vector<u8>{'I', 'P', 'S', '3', '2'} : std::vector<u8>{'P', 'A', 'T', 'C', 'H'};
    if (ips32) {
        ips.push_back(static_cast<u8>(offset >> 24));
    }
    ips.push_back(static_cast<u8>(offset >> 16));
    ips.push_back(static_cast<u8>(offset >> 8));
    ips.push_back(static_cast<u8>(offset));
    ips.push_back(static_cast<u8>(bytes.size() >> 8));
    ips.push_back(static_cast<u8>(bytes.size()));
    ips.insert(ips.end(), bytes.begin(), bytes.end());
    if (ips32) {
        ips.push_back('E');
    }
    ips.insert(ips.end(), {'E', 'O', 'F'});
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char*>(ips.data()), static_cast<std::streamsize>(ips.size()));
}

class PatchManagerTestContext {
public:
    explicit PatchManagerTestContext(std::optional<u32> user_mod_word = std::nullopt,
                                     size_t user_mod_offset = PatchOffset) {
        for (size_t i = 0; i < TestDirectories.size(); ++i) {
            const auto& [path, name] = TestDirectories[i];
            previous[i] = Common::FS::GetEdenPath(path);
            std::filesystem::create_directories(root / name);
            Common::FS::SetEdenPath(path, root / name);
        }
        const auto title_dir = root / "load" / fmt::format("{:016X}", OcarinaTitleId);
        std::filesystem::create_directories(title_dir);
        if (user_mod_word) {
            const auto bytes = std::bit_cast<std::array<u8, sizeof(u32)>>(*user_mod_word);
            AddUserPatch(bytes, user_mod_offset);
        }
        system.SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
        system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
        system.GetFileSystemController().CreateFactories(*system.GetFilesystem());
    }

    ~PatchManagerTestContext() {
        for (size_t i = 0; i < TestDirectories.size(); ++i) {
            Common::FS::SetEdenPath(TestDirectories[i].first, previous[i]);
        }
        Settings::values.disabled_addons.erase(OcarinaTitleId);
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    PatchManagerTestContext(const PatchManagerTestContext&) = delete;
    PatchManagerTestContext& operator=(const PatchManagerTestContext&) = delete;

    void AddUserPatch(std::span<const u8> bytes, size_t offset) {
        const auto exefs = root / "load" / fmt::format("{:016X}", OcarinaTitleId) / "UserMod" / "exefs";
        std::filesystem::create_directories(exefs);
        WriteIps(exefs / fmt::format("{}.{}.ips", OcarinaBuildIdHex, offset), bytes, offset);
    }

    FileSys::PatchManager Manager(u64 title_id = OcarinaTitleId) {
        return FileSys::PatchManager{title_id, system.GetFileSystemController(),
                                     system.GetContentProvider()};
    }

private:
    std::array<std::filesystem::path, TestDirectories.size()> previous;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("builtin-patch-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Core::System system;
};

std::vector<u8> MakeMain(const FileSys::PatchManager::BuildID& build_id, u32 word) {
    std::vector<u8> nso(PatchOffset + 0x100);
    Loader::NSOHeader header{};
    header.magic = Common::MakeMagic('N', 'S', 'O', '0');
    header.build_id = build_id;
    std::memcpy(nso.data(), &header, sizeof(header));
    std::memcpy(nso.data() + PatchOffset, &word, sizeof(word));
    return nso;
}

u32 WordAt(const std::vector<u8>& nso, size_t offset = PatchOffset) {
    REQUIRE(nso.size() >= offset + sizeof(u32));
    u32 word;
    std::memcpy(&word, nso.data() + offset, sizeof(word));
    return word;
}

u32 PatchMain(PatchManagerTestContext& context, const FileSys::PatchManager::BuildID& build_id,
              u32 word, const std::string& name = "main", u64 title_id = OcarinaTitleId) {
    return WordAt(context.Manager(title_id).PatchNSO(MakeMain(build_id, word), name));
}

} // Anonymous namespace

TEST_CASE("Built-in Max Resolution patch is gated on build id and original word",
          "[patch_manager]") {
    PatchManagerTestContext context;

    SECTION("Matching build id and original word applies") {
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction) == BranchInstruction);
    }

    SECTION("Wrong original word skips") {
        REQUIRE(PatchMain(context, OcarinaBuildId, NopInstruction) == NopInstruction);
    }

    SECTION("Other build id skips") {
        auto other = OcarinaBuildId;
        other[0] ^= 1;
        REQUIRE(PatchMain(context, other, TbzInstruction) == TbzInstruction);
    }

    SECTION("Other module skips") {
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction, "sdk") == TbzInstruction);
    }

    SECTION("Other title skips") {
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction, "main", OtherTitleId) ==
                TbzInstruction);
    }

    SECTION("Disabled in Add-Ons skips") {
        Settings::values.disabled_addons[OcarinaTitleId] = {std::string{MaxResolution}};
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction) == TbzInstruction);
    }
}

TEST_CASE("Built-in Max Resolution patch defers to a user mod at the same offset",
          "[patch_manager]") {
    SECTION("A different word from the mod is kept") {
        PatchManagerTestContext context{NopInstruction};
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction) == NopInstruction);
    }

    SECTION("The same word from the mod is applied once") {
        PatchManagerTestContext context{BranchInstruction};
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction) == BranchInstruction);
    }

    SECTION("Disabling the built-in patch keeps the mod") {
        PatchManagerTestContext context{BranchInstruction};
        Settings::values.disabled_addons[OcarinaTitleId] = {std::string{MaxResolution}};
        REQUIRE(PatchMain(context, OcarinaBuildId, TbzInstruction) == BranchInstruction);
    }
}

TEST_CASE("Built-in Max Resolution patch is listed and loaded for Ocarina's main",
          "[patch_manager]") {
    PatchManagerTestContext context;
    const auto manager = context.Manager();

    REQUIRE(manager.HasNSOPatch(OcarinaBuildId, "main"));
    REQUIRE_FALSE(manager.HasNSOPatch(OcarinaBuildId, "sdk"));
    REQUIRE_FALSE(context.Manager(OtherTitleId).HasNSOPatch(OcarinaBuildId, "main"));

    const auto builtin_enabled = [&] {
        const auto patches = manager.GetPatches();
        const auto it = std::ranges::find(patches, MaxResolution, &FileSys::Patch::name);
        REQUIRE(it != patches.end());
        REQUIRE(it->type == FileSys::PatchType::Mod);
        return it->enabled;
    };
    REQUIRE(builtin_enabled());

    Settings::values.disabled_addons[OcarinaTitleId] = {std::string{MaxResolution}};
    REQUIRE_FALSE(builtin_enabled());
}

namespace {

std::vector<u8> MakeDynamicMain() {
    auto nso = MakeMain(OcarinaBuildId, TbzInstruction);
    nso.resize(DynamicStateOffset + 16);
    Loader::NSOHeader header;
    std::memcpy(&header, nso.data(), sizeof(header));
    header.segments[0].size = 63201744;
    header.segments[1].location = 63205376;
    header.segments[1].size = 12603520;
    header.segments[2].location = 75812864;
    header.segments[2].size = 5217152;
    header.segments[2].bss_size = 1414272;
    std::memcpy(nso.data(), &header, sizeof(header));
    std::memcpy(nso.data() + DynamicCallOffset, &DynamicOriginalInstruction,
                sizeof(DynamicOriginalInstruction));
    return nso;
}

} // Anonymous namespace

TEST_CASE("Built-in Dynamic FPS defaults on and can be disabled in Add-Ons", "[patch_manager]") {
    PatchManagerTestContext context;
    const auto manager = context.Manager();
    const auto enabled = [&] {
        const auto patches = manager.GetPatches();
        const auto it = std::ranges::find(patches, DynamicFps, &FileSys::Patch::name);
        REQUIRE(it != patches.end());
        REQUIRE(it->type == FileSys::PatchType::Mod);
        return it->enabled;
    };
    REQUIRE(enabled());
    auto nso = MakeDynamicMain();
    auto patched = manager.PatchNSO(nso, "main");
    REQUIRE(WordAt(patched, DynamicCallOffset) == DynamicCallInstruction);
    REQUIRE(WordAt(patched, DynamicRoutineOffset) == 0x1E201828);
    REQUIRE(WordAt(patched, DynamicRoutineOffset + 116) == 0x3F83D70A);
    REQUIRE(
        Common::HexToString(std::span{patched}.subspan(DynamicRoutineOffset, DynamicRoutineSize)) ==
        "2818201ECA9200B04A0138912BE03BD54C0140F94B0100F9CC0200B46B010CCB0DE03BD5"
        "4E0180D2AE09CE9A7F010EEBCB818B9A4C0540F94C0000B5EC030BAA6E010CCB8C0D8E8B"
        "4C0500F98201239EA301239E4218231E4208211EC300001C0309231E4020231E4D000054"
        "4840201EC0035FD60AD7833F");
    REQUIRE(WordAt(patched) == BranchInstruction);
    Settings::values.disabled_addons[OcarinaTitleId] = {std::string{DynamicFps}};
    REQUIRE_FALSE(enabled());
    REQUIRE(WordAt(manager.PatchNSO(nso, "main"), DynamicCallOffset) == DynamicOriginalInstruction);
    REQUIRE(WordAt(manager.PatchNSO(nso, "main"), DynamicRoutineOffset) == 0);

    Settings::values.disabled_addons[OcarinaTitleId] = {std::string{MaxResolution}};
    REQUIRE(enabled());
    patched = manager.PatchNSO(nso, "main");
    REQUIRE(WordAt(patched, DynamicCallOffset) == DynamicCallInstruction);
    REQUIRE(WordAt(patched) == TbzInstruction);

    const auto unrelated = context.Manager(OtherTitleId).GetPatches();
    REQUIRE(std::ranges::find(unrelated, DynamicFps, &FileSys::Patch::name) == unrelated.end());
}

TEST_CASE("Built-in Dynamic FPS rejects mismatched or truncated modules transactionally",
          "[patch_manager]") {
    PatchManagerTestContext context;
    Settings::values.disabled_addons[OcarinaTitleId] = {std::string{MaxResolution}};
    auto nso = MakeDynamicMain();
    std::string module = "main";
    u64 title = OcarinaTitleId;
    SECTION("Wrong build") {
        nso[offsetof(Loader::NSOHeader, build_id)] ^= 1;
    }
    SECTION("Wrong module") {
        module = "sdk";
    }
    SECTION("Wrong title") {
        title = OtherTitleId;
    }
    SECTION("Incomplete header") {
        nso.resize(sizeof(Loader::NSOHeader) - 1);
    }
    SECTION("Incomplete call") {
        nso.resize(DynamicCallOffset + 3);
    }
    SECTION("Incomplete routine") {
        nso.resize(DynamicRoutineOffset + DynamicRoutineSize - 1);
    }
    SECTION("Incomplete timer state") {
        nso.resize(DynamicStateOffset + 15);
    }
    SECTION("Timer state outside BSS") {
        Loader::NSOHeader header;
        std::memcpy(&header, nso.data(), sizeof(header));
        header.segments[2].bss_size = 15;
        std::memcpy(nso.data(), &header, sizeof(header));
    }
    SECTION("Wrong original hook") {
        nso[DynamicCallOffset] ^= 1;
    }
    SECTION("Nonzero code cave") {
        nso[DynamicRoutineOffset + DynamicRoutineSize - 1] = 1;
    }
    SECTION("Nonzero timer state") {
        nso[DynamicStateOffset + 15] = 1;
    }
    SECTION("Code cave outside executable padding") {
        Loader::NSOHeader header;
        std::memcpy(&header, nso.data(), sizeof(header));
        header.segments[0].size = 0x3C46000;
        std::memcpy(nso.data(), &header, sizeof(header));
    }
    SECTION("Code cave overlaps read-only segment") {
        Loader::NSOHeader header;
        std::memcpy(&header, nso.data(), sizeof(header));
        header.segments[1].location = 0x3C46200;
        std::memcpy(nso.data(), &header, sizeof(header));
    }
    REQUIRE(context.Manager(title).PatchNSO(nso, module) == nso);
}

TEST_CASE("Built-in Dynamic FPS leaves both records alone when a user mod changes either",
          "[patch_manager]") {
    size_t offset = DynamicCallOffset;
    u32 mod_word = 0x11223344;
    SECTION("User call takes precedence") {}
    SECTION("Identical call still takes precedence for the whole patch") {
        mod_word = DynamicCallInstruction;
    }
    SECTION("User routine takes precedence") {
        offset = DynamicRoutineOffset;
    }
    SECTION("Last routine byte takes precedence") {
        offset = DynamicRoutineOffset + DynamicRoutineSize - sizeof(u32);
    }
    SECTION("User timer state takes precedence") {
        offset = DynamicStateOffset;
    }
    PatchManagerTestContext context{mod_word, offset};
    Settings::values.disabled_addons[OcarinaTitleId] = {std::string{MaxResolution}};
    const auto nso = MakeDynamicMain();
    auto expected = nso;
    std::memcpy(expected.data() + offset, &mod_word, sizeof(mod_word));
    REQUIRE(context.Manager().PatchNSO(nso, "main") == expected);
}

TEST_CASE("Ocarina high refresh is opt-in and reports only a complete applied patch",
          "[patch_manager]") {
    constexpr auto ScaleOffset = DynamicRoutineOffset + 0x88;
    constexpr auto WaterHookOffset = sizeof(Loader::NSOHeader) + 0x475F8;
    constexpr auto DynamicWaterRoutineOffset = sizeof(Loader::NSOHeader) + 0x3C46E90;
    constexpr u32 WaterOriginal = 0x1E380009;
    constexpr u32 WaterHook = 0x14EFFB26;
    constexpr u32 DynamicWaterHook = 0x14EFFE26;
    constexpr std::string_view WaterRipples = "Water ripples at any frame rate (built-in)";
    auto& enabled = Settings::values.oot_high_refresh;
    struct RestoreSetting {
        bool previous;
        ~RestoreSetting() {
            Settings::values.oot_high_refresh.SetValue(previous);
        }
    } restore{enabled.GetValue()};

    PatchManagerTestContext context;
    const auto manager = context.Manager();
    auto nso = MakeDynamicMain();
    nso.resize(DynamicStateOffset + 0xA0);
    std::memcpy(nso.data() + WaterHookOffset, &WaterOriginal, sizeof(WaterOriginal));
    size_t scale_offset{1};
    enabled.SetValue(false);
    const auto baseline = manager.PatchNSO(nso, "main", &scale_offset);
    REQUIRE(scale_offset == 0);
    REQUIRE(WordAt(baseline, DynamicCallOffset) == DynamicCallInstruction);
    REQUIRE(WordAt(baseline, WaterHookOffset) == DynamicWaterHook);
    REQUIRE(WordAt(baseline, DynamicWaterRoutineOffset) == 0xB00092C9);
    REQUIRE(WordAt(baseline, DynamicWaterRoutineOffset + 124) == 0x171001BC);
    REQUIRE(WordAt(baseline, ScaleOffset) == 0);

    SECTION("Disabling the water add-on keeps normal Dynamic FPS") {
        Settings::values.disabled_addons[OcarinaTitleId] = {std::string{WaterRipples}};
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(WordAt(patched, DynamicCallOffset) == DynamicCallInstruction);
        CHECK(WordAt(patched, WaterHookOffset) == WaterOriginal);
        CHECK(WordAt(patched, DynamicWaterRoutineOffset) == 0);
        return;
    }
    SECTION("Without an FPS patch, the water add-on leaves the original code") {
        Settings::values.disabled_addons[OcarinaTitleId] = {std::string{DynamicFps}};
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(WordAt(patched, DynamicCallOffset) == DynamicOriginalInstruction);
        CHECK(WordAt(patched, WaterHookOffset) == WaterOriginal);
        CHECK(WordAt(patched, DynamicWaterRoutineOffset) == 0);
        return;
    }
    SECTION("A user mod can occupy the normal ripple routine without disabling Dynamic FPS") {
        const auto word = std::bit_cast<std::array<u8, sizeof(u32)>>(NopInstruction);
        context.AddUserPatch(word, DynamicWaterRoutineOffset);
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(WordAt(patched, DynamicCallOffset) == DynamicCallInstruction);
        CHECK(WordAt(patched, WaterHookOffset) == WaterOriginal);
        CHECK(WordAt(patched, DynamicWaterRoutineOffset) == NopInstruction);
        return;
    }

    enabled.SetValue(true);
    SECTION("The matching build gets both hooks and the initial scale") {
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(scale_offset == ScaleOffset - sizeof(Loader::NSOHeader));
        CHECK(WordAt(patched, ScaleOffset) == 0x3F800000);
        CHECK(WordAt(patched, DynamicCallOffset) == DynamicCallInstruction);
        CHECK(WordAt(patched, DynamicRoutineOffset + 0x78) == 0x1E230848);
        CHECK(WordAt(patched, WaterHookOffset) == WaterHook);
        CHECK(WordAt(patched, DynamicRoutineOffset + 0x10C) == 0x171004BC);
        CHECK(WordAt(patched, DynamicWaterRoutineOffset) == 0);
    }
    SECTION("A conflicting ripple hook leaves the user mod and the previous Dynamic FPS patch") {
        const auto word = std::bit_cast<std::array<u8, sizeof(u32)>>(NopInstruction);
        context.AddUserPatch(word, WaterHookOffset);
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(scale_offset == 0);
        CHECK(WordAt(patched, WaterHookOffset) == NopInstruction);
        CHECK(WordAt(patched, DynamicRoutineOffset + 116) == 0x3F83D70A);
        CHECK(WordAt(patched, ScaleOffset) == 0);
    }
    SECTION("The standalone Dark patch can be upgraded without removing its files") {
        const auto bytes = std::span{baseline};
        context.AddUserPatch(bytes.subspan(DynamicCallOffset, sizeof(u32)), DynamicCallOffset);
        const auto routine = bytes.subspan(DynamicRoutineOffset, DynamicRoutineSize);
        std::vector<u8> dark{routine.begin(), routine.end()};
        dark[0x24] = 0x8E;
        dark[0x25] = 0x02;
        context.AddUserPatch(dark, DynamicRoutineOffset);
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(scale_offset == ScaleOffset - sizeof(Loader::NSOHeader));
        CHECK(WordAt(patched, WaterHookOffset) == WaterHook);
    }
    SECTION("An incompatible Build ID leaves the module untouched") {
        nso[offsetof(Loader::NSOHeader, build_id)] ^= 1;
        CHECK(manager.PatchNSO(nso, "main", &scale_offset) == nso);
        CHECK(scale_offset == 0);
    }
    SECTION("Truncating the extra ripple state falls back to the previous Dynamic FPS patch") {
        nso.resize(DynamicStateOffset + 16);
        const auto patched = manager.PatchNSO(nso, "main", &scale_offset);
        CHECK(scale_offset == 0);
        CHECK(WordAt(patched, DynamicRoutineOffset + 116) == 0x3F83D70A);
        CHECK(WordAt(patched, WaterHookOffset) == WaterOriginal);
    }
}
