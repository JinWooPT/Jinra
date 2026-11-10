// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <windows.h>

#include "common/logging.h"
#include "common/scope_exit.h"
#include "common/thread.h"
#endif

#include "core/device_memory.h"
#include "hle/kernel/board/nintendo/nx/k_system_control.h"

namespace Core {

#ifdef HAS_NCE
constexpr size_t VirtualReserveSize = 1ULL << 38;
#else
constexpr size_t VirtualReserveSize = 1ULL << 39;
#endif

DeviceMemory::DeviceMemory()
    : buffer{Kernel::Board::Nintendo::Nx::KSystemControl::Init::GetIntendedMemorySize(),
             VirtualReserveSize} {
#ifdef _WIN32
    if (buffer.BackingBasePointer() != nullptr) {
        try {
            zero_trim_thread = std::jthread{[this](std::stop_token stop) { TrimZeroPages(stop); }};
        } catch (const std::system_error& error) {
            LOG_WARNING(HW_Memory, "Could not start guest memory trimming: {}", error.what());
        }
    }
#endif
}

DeviceMemory::~DeviceMemory() = default;

#ifdef _WIN32
void DeviceMemory::TrimZeroPages(std::stop_token stop) {
    // Based on Lichii #961 and Ghostwolf13's zero-page working-set trim in #431.
    using namespace std::chrono_literals;
    Common::SetCurrentThreadName("ZeroTrim");
    Common::SetCurrentThreadPriority(Common::ThreadPriority::Low);

    // A working-set scan copies resident guest pages. Avoid that traffic while RAM is available.
    const HANDLE low_memory = CreateMemoryResourceNotification(LowMemoryResourceNotification);
    if (!low_memory) {
        LOG_WARNING(HW_Memory, "Windows guest memory trim unavailable (error {})", GetLastError());
        return;
    }
    SCOPE_EXIT { CloseHandle(low_memory); };
    LOG_INFO(HW_Memory, "Windows guest memory trim: waiting for low physical memory");

    constexpr size_t Busy = size_t{64} << 20;
    std::mutex mutex;
    std::condition_variable_any wake;
    auto interval = 5s;
    while (true) {
        {
            std::unique_lock lock{mutex};
            wake.wait_for(lock, stop, interval, [] { return false; });
        }
        if (stop.stop_requested()) {
            return;
        }
        BOOL under_pressure{};
        if (!QueryMemoryResourceNotification(low_memory, &under_pressure)) {
            LOG_WARNING(HW_Memory, "Windows guest memory trim query failed (error {})", GetLastError());
            return;
        }
        if (!under_pressure) {
            interval = 5s;
            continue;
        }
        LOG_INFO(HW_Memory, "Windows low physical memory: scanning resident guest pages");
        const auto start = std::chrono::steady_clock::now();
        const auto [resident, trimmed] = buffer.TrimZeroPages(stop);
        interval = trimmed >= Busy ? 5s : (std::min)(interval * 2, 60s);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        LOG_INFO(HW_Memory, "Trimmed {} MiB of all-zero guest pages out of {} MiB resident in {} ms",
                 trimmed >> 20, resident >> 20, elapsed.count());
    }
}
#endif

} // namespace Core
