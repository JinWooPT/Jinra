// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Common::Linux {

/// On a crash signal (SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT) writes one "Host crash" line
/// (signal, faulting address, thread name) and the stack (backtrace_symbols_fd: binary+offset)
/// straight into the log file and stderr, then lets the signal take its default action (core
/// dump). Linux had no crash report at all: a crash ended the log mid-session with nothing in it
/// (Steam Deck after shader preparation, 2026-11-18). Call after Common::Log::Start and before
/// the JIT installs its own SIGSEGV handler (dynarmic chains to this one for faults outside its
/// code).
void InstallCrashHandler();

} // namespace Common::Linux
