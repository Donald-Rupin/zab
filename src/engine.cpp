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
 *  @file engine.cpp
 *
 */

#include "zab/engine.hpp"

#include <fstream>
#include <iterator>
#include <latch>
#include <sched.h>
#include <string>
#include <thread>

#include "zab/async_function.hpp"
#include "zab/yield.hpp"

namespace zab {

    namespace {

        async_function<>
        do_function(engine* _this, std::function<void()> _func, order_t _order, thread_t _thread)
        {
            co_await yield(_this, _order, _thread);

            _func();
        }
    }   // namespace

    thread_local thread_t engine::this_thead_ = thread_t{};

    engine::engine(configs _configs)
        : event_loop_(validate(_configs)), sig_handler_(this), configs_(_configs)
    { }

    std::uint16_t
    engine::validate(configs& _configs)
    {
        auto cores = core_count();

        if (_configs.opt_ == configs::kAny) { _configs.threads_ = cores; }
        else if (_configs.opt_ == configs::kAtLeast)
        {
            _configs.threads_ = std::max(cores, _configs.threads_);
        }

        if (!_configs.threads_) { _configs.threads_ = 1; }

        return _configs.threads_;
    }

    std::uint16_t
    engine::core_count() noexcept
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
    engine::set_worker_affinity(thread_t _thread_id) noexcept
    {
        if (_thread_id < threads_.size())
        {
            auto      count = core_count();
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET((_thread_id.thread_ + configs_.affinity_offset_) % (count), &cpuset);
            int rc = pthread_setaffinity_np(
                threads_[_thread_id.thread_].native_handle(),
                sizeof(cpu_set_t),
                &cpuset);
            if (rc != 0) { std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n"; }
        }
    }

    void
    engine::start() noexcept
    {
        /* The main thread can just use the first one. */
        /* There will be no race since this thread will block until */
        /* the other threads start.  */
        this_thead_ = thread_t{0};

        std::latch lat(configs_.threads_ + 1);

        {
            std::scoped_lock lck(mtx_);
            for (std::uint16_t i = 0; i < configs_.threads_; ++i)
            {
                timers_.emplace_back(this);
                threads_.emplace_back(
                    [this, &lat, i](auto _stop_token)
                    {
                        this_thead_ = thread_t{i};
                        std::stop_callback callback(
                            _stop_token,
                            event_loop_[i].get_stop_function());

                        lat.arrive_and_wait();

                        if (i == signal_handler::kSignalThread) { sig_handler_.run(); }
                        timers_[i].run();

                        event_loop_[i].run(_stop_token);
                    });
            }
        }

        lat.arrive_and_wait();

        for (auto& t : threads_)
        {
            if (t.joinable()) { t.join(); }
        }

        std::scoped_lock lck(mtx_);
        threads_.clear();
        timers_.clear();
    }

    void
    engine::stop() noexcept
    {
        sig_handler_.stop();
        std::scoped_lock lck(mtx_);
        for (auto& t : threads_)
        {
            t.request_stop();
        }
    }

    void
    engine::execute(std::function<void()> _yielder, order_t _order, thread_t _thread) noexcept
    {
        do_function(this, std::move(_yielder), _order, _thread);
    }

    void
    engine::resume(event _handle) noexcept
    {
        thread_resume(_handle, this_thead_);
    }

    thread_t
    engine::get_any_thread()
    {
        thread_t      thread;
        std::uint16_t current  = 0;
        std::size_t   min_size = std::numeric_limits<std::size_t>::max();
        for (const auto& el : event_loop_)
        {
            auto es = el.event_size();
            if (!es)
            {
                thread.thread_ = current;
                break;
            }
            else if (es < min_size)
            {
                min_size       = es;
                thread.thread_ = current;
            }

            ++current;
        }

        return thread;
    }

    void
    engine::thread_resume(event _handle, thread_t _thread) noexcept
    {
        if (_thread.thread_ == thread_t::kAnyThread) { _thread = get_any_thread(); }

        assert(_thread.thread_ < event_loop_.size());

        event_loop_[_thread.thread_].user_event(_handle);
    }

    void
    engine::delayed_resume(event _handle, order_t _order) noexcept
    {

        delayed_resume(_handle, _order, this_thead_);
    }

    void
    engine::delayed_resume(event _handle, order_t _order, thread_t _thread) noexcept
    {
        if (_thread.thread_ == thread_t::kAnyThread) { _thread = get_any_thread(); }

        if (_order.order_)
        {
            assert(this_thead_ != thread::any());
            timers_[this_thead_.thread_].wait(_handle, _order.order_, _thread);
        }
        else
        {
            thread_resume(_handle, _thread);
        }
    }

}   // namespace zab