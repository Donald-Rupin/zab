/*
 *  MMM"""AMV       db      `7MM"""Yp,
 *  M'   AMV       ;MM:       MM    Yb
 *  '   AMV       ,V^MM.      MM    dP
 *     AMV       ,M  `MM      MM"""bg.
 *    AMV   ,    AbmmmqMA     MM    `Y
 *   AMV   ,M   A'     VML    MM    ,9
 *  AMVmmmmMM .AMA.   .AMMA..JMMmmmd9
 *
 *
 * MIT License
 *
 * Copyright (c) 2021 Donald-Rupin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 *  @file event_loop.cpp
 *
 */

#include "zab/event_loop.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <sched.h>
#include <thread>
// TODO(donald) use latch when widely avaible. Just uncomment the code.
//#include <latch>

namespace zab {

    event_loop::event_loop(configs _configs) : workers_(validate(_configs)), configs_(_configs) { }

    event_loop::~event_loop() { }

    std::uint16_t
    event_loop::validate(configs& _configs)
    {
        if (_configs.opt_ == configs::kAny) { _configs.threads_ = core_count() - 1; }
        else if (_configs.opt_ == configs::kAtLeast)
        {

            auto cores = core_count();

            if (cores) { --cores; }

            _configs.threads_ = std::max(cores, _configs.threads_);
        }

        if (!_configs.threads_) { _configs.threads_ = 1; }

        return _configs.threads_;
    }

    std::uint16_t
    event_loop::core_count() noexcept
    {
        auto count = std::thread::hardware_concurrency();

        if (!count)
        {
            /* See if we can look up in proc (nix only) */
            /* TODO(donald): Im not sure if cpu info format is standard */
            /*       Or if it includes virtual (hyper) cores */
            /* TODO(donald): windows impl */
            std::ifstream cpuinfo("/proc/cpuinfo");
            count = std::count(
                std::istream_iterator<std::string>(cpuinfo),
                std::istream_iterator<std::string>(),
                std::string("processor"));
        }

        if (count > std::numeric_limits<std::uint16_t>::max()) [[unlikely]]
        {
            count = std::numeric_limits<std::uint16_t>::max();
        }

        return count;
    }

    void
    event_loop::set_affinity(thread_t _thread_id) noexcept
    {
        auto      count = core_count();
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET((_thread_id.thread_ + configs_.affinity_offset_) % (count), &cpuset);
        int rc = pthread_setaffinity_np(
            workers_[_thread_id.thread_].second.native_handle(),
            sizeof(cpu_set_t),
            &cpuset);
        if (rc != 0) { std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n"; }
    }

    void
    event_loop::start() noexcept
    {
        /* TODO: use latch instead of this rubish... */
        // std::latch wait_for_init{static_cast<std::ptrdiff_t>(workers_.size())};

        ids_.resize(workers_.size());

        std::mutex latchy;
        {
            std::scoped_lock lck(latchy);
            for (std::uint16_t count = 0; auto& [work, thread] : workers_)
            {

                thread = std::jthread(
                    [this, count, /* &wait_for_init */ &latchy](std::stop_token _stop_token)
                    {
                        // wait_for_init.arrive_and_wait();
                        {
                            std::scoped_lock lck(latchy);
                        }

                        if (configs_.affinity_set_) { set_affinity(thread_t{count}); }

                        run_loop(_stop_token, thread_t{count});
                    });

                ids_[count] = thread.get_id();

                ++count;
            }
        }

        for (auto& [work, thread] : workers_)
        {
            if (thread.joinable()) { thread.join(); }
        }
    }

    void
    event_loop::stop() noexcept
    {
        for (auto& [work, thread] : workers_)
        {
            if (!thread.get_stop_token().stop_requested()) { thread.request_stop(); }
        }
    }

    void
    event_loop::purge() noexcept
    {
        stop();

        for (auto& [work, thread] : workers_)
        {
            if (thread.joinable()) { thread.join(); }
        }

        workers_.clear();
    }

    void
    event_loop::send_event(event&& _event, order_t ordering_, thread_t _thread_number) noexcept
    {
        if (_thread_number == kAnyThread)
        {
            /* Pick the one with the lowest load */
            _thread_number.thread_ = 0;
            size_t size            = std::numeric_limits<size_t>::max();
            for (size_t count = 0; auto& [work, thread] : workers_)
            {
                size_t amount = work.size_.load(std::memory_order_acquire);
                if (!amount)
                {
                    _thread_number.thread_ = count;
                    break;
                }
                if (size > amount)
                {
                    size                   = amount;
                    _thread_number.thread_ = count;
                }
            }
        }

        auto& worker = workers_[_thread_number.thread_].first;

        std::unique_lock lck(worker.events_mtx_);

        worker.size_.fetch_add(1, std::memory_order_acquire);
        worker.events_.emplace(
            std::upper_bound(
                worker.events_.begin(),
                worker.events_.end(),
                ordering_,
                [](order_t _order, const std::pair<event, order_t>& _event) -> bool
                { return _order < _event.second; }),
            std::make_pair(std::move(_event), ordering_));

        worker.events_cv_.notify_one();
    }

    void
    event_loop::run_loop(std::stop_token _stop_token, thread_t _thread_number) noexcept
    {
        auto& worker = workers_[_thread_number.thread_].first;

        std::stop_callback callback(
            _stop_token,
            [&worker]
            {
                std::unique_lock lck(worker.events_mtx_);
                worker.events_.emplace_back(event{}, order_t{});
                worker.events_cv_.notify_all();
            });

        while (!_stop_token.stop_requested())
        {
            if (worker.size_.load(std::memory_order_acquire))
            {

                std::unique_lock lck(worker.events_mtx_);

                auto time = std::chrono::time_point<std::chrono::high_resolution_clock>(
                    std::chrono::duration<std::uint64_t, std::nano>(
                        worker.events_.front().second.order_));

                worker.events_cv_.wait_for(
                    lck,
                    time - std::chrono::high_resolution_clock::now(),
                    [&worker]()
                    {
                        auto time = std::chrono::time_point<std::chrono::high_resolution_clock>(
                            std::chrono::duration<std::uint64_t, std::nano>(
                                worker.events_.front().second.order_));

                        return std::chrono::high_resolution_clock::now() >= time;
                    });
            }
            else
            {

                std::unique_lock lck(worker.events_mtx_);

                worker.events_cv_.wait(lck, [&worker]() { return worker.events_.size(); });
            }

            while (!_stop_token.stop_requested())
            {

                event event;
                {
                    if (worker.size_.load(std::memory_order_acquire))
                    {

                        std::unique_lock lck(worker.events_mtx_);

                        auto time = std::chrono::time_point<std::chrono::high_resolution_clock>(
                            std::chrono::duration<std::uint64_t, std::nano>(
                                worker.events_.front().second.order_));

                        if (std::chrono::high_resolution_clock::now() >= time)
                        {

                            std::swap(event, worker.events_.front().first);
                            worker.events_.pop_front();
                            worker.size_.fetch_sub(1, std::memory_order_release);
                        }
                        else
                        {

                            break;
                        }
                    }
                    else
                    {

                        break;
                    }
                }

                std::visit(
                    [this, _thread_number](auto& _type) { process_event(_thread_number, _type); },
                    event.type_);
            }
        }
    }

    thread_t
    event_loop::current_id() const noexcept
    {
        auto this_tid = std::this_thread::get_id();
        /* Presumably cbegin, cend and operator[]const are thread safe.*/
        for (std::uint16_t count = 0; const auto& stdtid : ids_)
        {

            /* operator= const should also be thread safe. */
            if (this_tid == stdtid) { return thread_t{count}; }

            ++count;
        }

        return kAnyThread;
    }

}   // namespace zab