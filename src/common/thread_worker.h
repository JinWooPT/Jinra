// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "common/unique_function.h"

namespace Common {

template <class StateType = void>
class StatefulThreadWorker {
    static constexpr bool with_state = !std::is_same_v<StateType, void>;

    struct DummyCallable {
        int operator()() const noexcept {
            return 0;
        }
    };

    using Task =
        std::conditional_t<with_state, UniqueFunction<void, StateType*>, UniqueFunction<void>>;
    using StateMaker = std::conditional_t<with_state, std::function<StateType()>, DummyCallable>;

public:
    explicit StatefulThreadWorker(size_t num_workers, std::string name, StateMaker func = {},
                                  ThreadPlacement placement = ThreadPlacement::Default)
        : workers_queued{num_workers}, thread_name{std::move(name)} {
        const auto lambda = [this, func, placement](std::stop_token stop_token) {
            Common::SetCurrentThreadName(thread_name.c_str());
            if (placement != ThreadPlacement::Default) {
                Common::SetCurrentThreadPriority(ThreadPriority::Low);
            }
            switch (placement) {
            case ThreadPlacement::Efficiency:
                Common::SetCurrentThreadToEfficiencyCores();
                break;
            case ThreadPlacement::Background:
                Common::SetCurrentThreadToBackgroundWork();
                break;
            default:
                Common::SetCurrentThreadToAllCores();
                break;
            }
            {
                [[maybe_unused]] std::conditional_t<with_state, StateType, int> state{func()};
                while (!stop_token.stop_requested()) {
                    Task task;
                    {
                        std::unique_lock lock{queue_mutex};
                        if (requests.empty()) {
                            wait_condition.notify_all();
                        }
                        condition.wait(lock, stop_token,
                                       [this] { return !requests.empty(); });
                        if (stop_token.stop_requested()) {
                            break;
                        }
                        task = std::move(requests.front().task);
                        requests.pop_front();
                    }
                    if constexpr (with_state) {
                        task(&state);
                    } else {
                        task();
                    }
                    ++work_done;
                }
            }
            {
                std::scoped_lock lock{queue_mutex};
                ++workers_stopped;
            }
            wait_condition.notify_all();
        };
        threads.reserve(num_workers);
        for (size_t i = 0; i < num_workers; ++i) {
            threads.emplace_back(lambda);
        }
    }

    StatefulThreadWorker& operator=(const StatefulThreadWorker&) = delete;
    StatefulThreadWorker(const StatefulThreadWorker&) = delete;

    StatefulThreadWorker& operator=(StatefulThreadWorker&&) = delete;
    StatefulThreadWorker(StatefulThreadWorker&&) = delete;

    size_t QueueWork(Task work) {
        size_t ticket;
        {
            std::unique_lock lock{queue_mutex};
            ticket = Enqueue(std::move(work));
        }
        condition.notify_one();
        return ticket;
    }

    bool PrioritizeWork(size_t ticket) {
        std::scoped_lock lock{queue_mutex};
        const auto first_normal = std::find_if(requests.begin(), requests.end(),
                                               [](const auto& entry) { return !entry.priority; });
        const auto found = std::find_if(first_normal, requests.end(),
                                        [ticket](const auto& entry) { return entry.ticket == ticket; });
        if (found == requests.end()) {
            return false;
        }
        found->priority = true;
        std::rotate(first_normal, found, found + 1);
        return found != first_normal;
    }

    bool TryQueueWork(Task work) {
        {
            std::unique_lock lock{queue_mutex};
            if (work_scheduled - work_done >= workers_queued) {
                return false;
            }
            Enqueue(std::move(work));
        }
        condition.notify_one();
        return true;
    }

    void WaitForRequests(std::stop_token stop_token = {}) {
        std::stop_callback callback(stop_token, [this] {
            for (auto& thread : threads) {
                thread.request_stop();
            }
        });
        std::unique_lock lock{queue_mutex};
        wait_condition.wait(lock, [this] {
            return workers_stopped >= workers_queued || work_done >= work_scheduled;
        });
    }

private:
    size_t Enqueue(Task work) {
        const size_t ticket = work_scheduled.load(std::memory_order_relaxed) + 1;
        requests.emplace_back(ticket, std::move(work));
        work_scheduled.store(ticket, std::memory_order_relaxed);
        return ticket;
    }

    struct QueuedTask {
        size_t ticket;
        Task task;
        bool priority{};
    };
    std::deque<QueuedTask> requests;
    std::mutex queue_mutex;
    std::condition_variable_any condition;
    std::condition_variable wait_condition;
    std::atomic<size_t> work_scheduled{};
    std::atomic<size_t> work_done{};
    std::atomic<size_t> workers_stopped{};
    std::atomic<size_t> workers_queued{};
    std::string thread_name;
    std::vector<std::jthread> threads;
};

using ThreadWorker = StatefulThreadWorker<>;

} // namespace Common
