// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/device_memory.h"
#include "core/device_memory_manager.h"
#include "core/device_memory_manager.inc"

namespace Core {

// Seed a valid initial mapping without starting an emulated process or its kernel page table.
// All unmaps, cache lookups, block transfers and span queries below use the production methods.
struct DeviceMemoryManagerTestAccess {
    template <typename Traits>
    static void SeedContiguous(DeviceMemoryManager<Traits>& memory, DAddr address, PAddr physical,
                               size_t pages) {
        const size_t first = address / DEVICE_PAGESIZE;
        const size_t physical_first = physical / DEVICE_PAGESIZE;
        memory.tracked_entries.CommitRegion(first, first + pages);
        for (size_t index = 0; index < pages; ++index) {
            auto& entry = memory.tracked_entries.GetUnchecked(first + index);
            entry.cpu_backing_address = physical + index * DEVICE_PAGESIZE;
            entry.compressed_physical_ptr = static_cast<u32>(physical_first + index + 1);
            entry.continuity_tracker = static_cast<u32>(pages - index - 1);
            memory.compressed_device_addr.GetAndFault(physical_first + index) =
                static_cast<u32>(first + index);
        }
    }
};

} // namespace Core

namespace {

constexpr size_t PageSize = Core::DEVICE_PAGESIZE;
constexpr size_t PageCount = 6;
constexpr DAddr DeviceBase = 4 * PageSize;
constexpr PAddr PhysicalBase = 16 * PageSize;

struct TestDeviceInterface {
    void InvalidateRegion(DAddr address, size_t size) {
        invalidated_address = address;
        invalidated_size = size;
    }

    DAddr invalidated_address{};
    size_t invalidated_size{};
};

struct TestDeviceTraits {
    static constexpr size_t device_virtual_bits = 20;
    using DeviceInterface = TestDeviceInterface;
    struct DeviceMethods {};
};

using TestDeviceMemory = Core::DeviceMemoryManager<TestDeviceTraits>;

struct ContinuityFixture {
    ContinuityFixture() {
        memory.BindInterface(&device);
        Core::DeviceMemoryManagerTestAccess::SeedContiguous(memory, DeviceBase, PhysicalBase,
                                                          PageCount);
        for (size_t index = 0; index < PageCount; ++index) {
            std::fill_n(PhysicalPage(index), PageSize, static_cast<u8>(0x40 + index));
        }
    }

    u8* PhysicalPage(size_t index) {
        return backing.GetPointerFromRaw<u8>(PhysicalBase + index * PageSize);
    }

    std::vector<u8> Read(DAddr address, size_t size) {
        std::vector<u8> bytes(size, 0xff);
        memory.ReadBlockUnsafe(address, bytes.data(), bytes.size());
        return bytes;
    }

    Core::DeviceMemory backing;
    TestDeviceInterface device;
    TestDeviceMemory memory{backing};
};

void RequirePageValues(std::span<const u8> bytes, std::span<const u8> page_values) {
    REQUIRE(bytes.size() == page_values.size() * PageSize);
    for (size_t index = 0; index < page_values.size(); ++index) {
        INFO("page offset " << index);
        REQUIRE(std::ranges::all_of(bytes.subspan(index * PageSize, PageSize),
                                   [expected = page_values[index]](u8 value) {
                                       return value == expected;
                                   }));
    }
}

} // namespace

TEST_CASE("Device memory reads stop at an unmapped page inside a contiguous run",
          "[device-memory][continuity]") {
    ContinuityFixture fixture;
    REQUIRE(fixture.memory.GetSpan(DeviceBase, PageCount * PageSize) == fixture.PhysicalPage(0));
    RequirePageValues(fixture.Read(DeviceBase, PageCount * PageSize),
                      std::array<u8, PageCount>{0x40, 0x41, 0x42, 0x43, 0x44, 0x45});

    // Prime the single-page translation cache before removing its backing.
    u8 cached{};
    fixture.memory.ReadBlockUnsafe(DeviceBase + 2 * PageSize, &cached, 1);
    REQUIRE(cached == 0x42);
    fixture.memory.Unmap(DeviceBase + 2 * PageSize, PageSize);
    REQUIRE(fixture.device.invalidated_address == DeviceBase + 2 * PageSize);
    REQUIRE(fixture.device.invalidated_size == PageSize);
    fixture.memory.ReadBlockUnsafe(DeviceBase + 2 * PageSize, &cached, 1);
    REQUIRE(cached == 0);

    RequirePageValues(fixture.Read(DeviceBase, PageCount * PageSize),
                      std::array<u8, PageCount>{0x40, 0x41, 0, 0x43, 0x44, 0x45});
    REQUIRE(fixture.memory.GetSpan(DeviceBase, 3 * PageSize) == nullptr);
    REQUIRE(fixture.memory.GetSpan(DeviceBase, 2 * PageSize) == fixture.PhysicalPage(0));
    REQUIRE(fixture.memory.GetSpan(DeviceBase + 3 * PageSize, 3 * PageSize) ==
            fixture.PhysicalPage(3));
    const TestDeviceMemory& const_memory = fixture.memory;
    REQUIRE(const_memory.GetSpan(DeviceBase, 3 * PageSize) == nullptr);
    REQUIRE(const_memory.GetSpan(DeviceBase + 3 * PageSize, 3 * PageSize) ==
            fixture.PhysicalPage(3));
}

TEST_CASE("Device memory unmapping a run's first page preserves the mapped tail",
          "[device-memory][continuity]") {
    ContinuityFixture fixture;
    fixture.memory.Unmap(DeviceBase, PageSize);
    RequirePageValues(fixture.Read(DeviceBase, PageCount * PageSize),
                      std::array<u8, PageCount>{0, 0x41, 0x42, 0x43, 0x44, 0x45});
    REQUIRE(fixture.memory.GetSpan(DeviceBase, PageSize) == nullptr);
    REQUIRE(fixture.memory.GetSpan(DeviceBase + PageSize, (PageCount - 1) * PageSize) ==
            fixture.PhysicalPage(1));
}

TEST_CASE("Device memory resets every page of a multi-page hole",
          "[device-memory][continuity]") {
    ContinuityFixture fixture;
    fixture.memory.Unmap(DeviceBase + PageSize, 3 * PageSize);
    constexpr std::array<u8, PageCount> expected{0x40, 0, 0, 0, 0x44, 0x45};
    RequirePageValues(fixture.Read(DeviceBase, PageCount * PageSize), expected);
    for (size_t index = 1; index <= 3; ++index) {
        RequirePageValues(fixture.Read(DeviceBase + index * PageSize,
                                       (PageCount - index) * PageSize),
                          std::span{expected}.subspan(index));
    }
    REQUIRE(fixture.memory.GetSpan(DeviceBase + 4 * PageSize, 2 * PageSize) ==
            fixture.PhysicalPage(4));
}

TEST_CASE("Device memory block writes do not write through an unmapped hole",
          "[device-memory][continuity]") {
    ContinuityFixture fixture;
    fixture.memory.Unmap(DeviceBase + 2 * PageSize, PageSize);
    const std::vector<u8> replacement(PageCount * PageSize, 0x9e);
    fixture.memory.WriteBlockUnsafe(DeviceBase, replacement.data(), replacement.size());
    for (size_t index = 0; index < PageCount; ++index) {
        INFO("physical page offset " << index);
        const u8 expected = index == 2 ? 0x42 : 0x9e;
        REQUIRE(std::ranges::all_of(std::span{fixture.PhysicalPage(index), PageSize},
                                   [expected](u8 value) { return value == expected; }));
    }
    RequirePageValues(fixture.Read(DeviceBase, PageCount * PageSize),
                      std::array<u8, PageCount>{0x9e, 0x9e, 0, 0x9e, 0x9e, 0x9e});
}

TEST_CASE("Device memory cuts only the run that crosses an unmap boundary",
          "[device-memory][continuity]") {
    ContinuityFixture fixture;
    constexpr DAddr OtherBase = DeviceBase + PageCount * PageSize;
    constexpr PAddr OtherPhysical = PhysicalBase + 16 * PageSize;
    Core::DeviceMemoryManagerTestAccess::SeedContiguous(fixture.memory, OtherBase, OtherPhysical,
                                                      PageCount);
    fixture.memory.Unmap(OtherBase + 2 * PageSize, PageSize);
    REQUIRE(fixture.memory.GetSpan(DeviceBase, PageCount * PageSize) == fixture.PhysicalPage(0));
    REQUIRE(fixture.memory.GetSpan(OtherBase, 3 * PageSize) == nullptr);
    REQUIRE(fixture.memory.GetSpan(OtherBase, 2 * PageSize) ==
            fixture.backing.GetPointerFromRaw<u8>(OtherPhysical));
    REQUIRE(fixture.memory.GetSpan(OtherBase + 3 * PageSize, 3 * PageSize) ==
            fixture.backing.GetPointerFromRaw<u8>(OtherPhysical + 3 * PageSize));

    // The backward cut at device page zero must not underflow into another run.
    Core::DeviceMemoryManagerTestAccess::SeedContiguous(fixture.memory, 0,
                                                      OtherPhysical + 16 * PageSize, 2);
    fixture.memory.Unmap(0, PageSize);
    REQUIRE(fixture.memory.GetSpan(0, PageSize) == nullptr);
    REQUIRE(fixture.memory.GetSpan(DeviceBase, PageCount * PageSize) == fixture.PhysicalPage(0));
}
