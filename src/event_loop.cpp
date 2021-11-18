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
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <latch>
#include <limits>
#include <mutex>
#include <sched.h>
#include <thread>

namespace zab {

    event_loop::event_loop(configs _configs) : configs_(_configs)
    {
        auto number_of_workers = validate(_configs);
        while (workers_.size() != number_of_workers)
        {
            workers_.emplace_back(std::make_pair(safe_queue{}, std::jthread{}));
        }
    }

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
        std::latch wait_for_init{static_cast<std::ptrdiff_t>(workers_.size() + 1)};
        ids_.resize(workers_.size());

        for (std::uint16_t count = 0; auto& [work, thread] : workers_)
        {
            thread = std::jthread(
                [this, count, &wait_for_init](std::stop_token _stop_token)
                {
                    wait_for_init.arrive_and_wait();

                    if (configs_.affinity_set_) { set_affinity(thread_t{count}); }

                    run_loop(_stop_token, thread_t{count});
                });

            ids_[count] = thread.get_id();

            ++count;
        }

        wait_for_init.arrive_and_wait();

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
    event_loop::send_event(event _event, thread_t _thread_number) noexcept
    {
        if (_thread_number == kAnyThread)
        {
            std::size_t min = std::numeric_limits<std::size_t>::max();
            for (std::uint16_t count = 0; const auto& [w, _] : workers_)
            {
                auto current_size = w.size_.load(std::memory_order_relaxed);
                if (current_size < min)
                {
                    _thread_number.thread_ = count;

                    if (!current_size) { break; }
                }

                ++count;
            }
        }

        assert(_thread_number.thread_ < workers_.size());

        auto& [w, _] = workers_[_thread_number.thread_];

        {
            std::unique_lock lck(w.mtx_);
            w.events_.push_back(_event);
            w.size_.fetch_add(1, std::memory_order_relaxed);
            if (w.events_.size() == 1) { w.cv_.notify_one(); }
        }
    }

    void
    event_loop::run_loop(std::stop_token _stop_token, thread_t _thread_number) noexcept
    {
        auto& [w, _] = workers_[_thread_number.thread_];

        std::stop_callback callback(
            _stop_token,
            [this, _thread_number] { send_event(nullptr, _thread_number); });

        while (!_stop_token.stop_requested())
        {
            event e;
            {
                std::unique_lock lck(w.mtx_);
                if (!w.events_.size())
                {
                    w.cv_.wait(lck, [&w = w]() { return w.events_.size(); });
                }

                e = w.events_.front();
                w.events_.pop_front();
                w.size_.fetch_sub(1, std::memory_order_relaxed);
            }

            if (e) { e.resume(); }
        }
    }

    event_loop::safe_queue::safe_queue() : size_(0) { }

    event_loop::safe_queue::safe_queue(safe_queue&& _que)
        : size_(_que.size_.load()), events_(std::move(_que.events_))
    { }

    event_loop::safe_queue::~safe_queue()
    {
        for (const auto e : events_)
        {
            if (e) { e.destroy(); }
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