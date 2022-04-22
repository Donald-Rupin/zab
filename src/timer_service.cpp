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
 *  @file timer_service.cpp
 *
 */

#include "zab/timer_service.hpp"

#include <cstring>
#include <span>
#include <sys/timerfd.h>

#include "zab/engine.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/yield.hpp"

namespace zab {

    timer_service::timer_service(engine* _engine)
        : engine_(_engine), handle_(nullptr), read_buffer_(0), current_(0), timer_fd_(0)
    { }

    timer_service::timer_service(timer_service&& _other)
        : engine_(_other.engine_), handle_(_other.handle_), waiting_(std::move(_other.waiting_)),
          current_(_other.current_), timer_fd_(_other.timer_fd_)
    {
        _other.handle_   = nullptr;
        _other.timer_fd_ = 0;
    }

    timer_service::~timer_service()
    {
        if (handle_)
        {
            /* We only use coroutine handles here */
            std::get<std::coroutine_handle<>>(handle_->handle_).destroy();
        }

        if (timer_fd_)
        {
            if (::close(timer_fd_))
            {
                std::cerr << "zab::timer_service::~timer_service() Failed to close a timer_server "
                             "socket.\n";
            }
        }
    }

    async_function<>
    timer_service::run() noexcept
    {
        if (!timer_fd_)
        {
            timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

            if (timer_fd_ == -1)
            {
                std::cerr << "timer_service -> Failed to create timerfd. errno:" << errno << "\n";
                abort();
            }
        }

        struct itimerspec current_spec;
        while (timer_fd_)
        {
            auto rc = co_await engine_->get_event_loop().read(
                timer_fd_,
                std::span<std::byte>(
                    static_cast<std::byte*>(static_cast<void*>(&read_buffer_)),
                    sizeof(read_buffer_)),
                0,
                &handle_);
            handle_ = nullptr;

            if (rc == sizeof(read_buffer_))
            {
                if (read_buffer_)
                {
                    rc = timerfd_gettime(timer_fd_, &current_spec);
                    if (rc < 0)
                    {
                        std::cerr << "timer_service -> timerfd_gettime failed. errno:" << errno
                                  << "\n";
                        abort();
                    }

                    current_ +=
                        ((((std::uint64_t) current_spec.it_interval.tv_sec) * kNanoInSeconds) +
                         current_spec.it_interval.tv_nsec) *
                        read_buffer_;

                    for (auto it = waiting_.begin(); it != waiting_.end() && it->first <= current_;)
                    {
                        for (const auto& [handle, thread] : it->second)
                        {
                            engine_->thread_resume(handle, thread);
                        }

                        it = waiting_.erase(it);
                    }

                    if (!waiting_.size())
                    {
                        current_ = 0;
                        struct itimerspec new_value;
                        ::memset((char*) &new_value, 0, sizeof(new_value));

                        /* disarm timer */
                        auto rc = timerfd_settime(
                            timer_fd_,
                            0, /* relative */
                            &new_value,
                            nullptr);

                        if (rc < 0)
                        {
                            std::cerr
                                << "timer_service -> timerfd_settime failed (1). errno:" << errno
                                << "\n";
                            abort();
                        }
                    }
                    else
                    {
                        change_timer(waiting_.begin()->first - current_);
                    }
                }
            }
            else
            {
                break;
            }
        }
    }

    void
    timer_service::change_timer(std::uint64_t _nano_seconds) noexcept
    {
        struct itimerspec new_value;

        new_value.it_value.tv_sec  = _nano_seconds / kNanoInSeconds;
        new_value.it_value.tv_nsec = _nano_seconds % kNanoInSeconds;

        new_value.it_interval = new_value.it_value;

        auto rc = timerfd_settime(
            timer_fd_,
            0, /* relative */
            &new_value,
            nullptr);

        if (rc < 0)
        {
            std::cerr << "timer_service -> timerfd_settime failed (2). errno:" << errno << "\n";
            abort();
        }
    }

    void
    timer_service::wait(event<> _handle, std::uint64_t _nano_seconds) noexcept
    {
        wait(_handle, _nano_seconds, engine_->current_id());
    }

    void
    timer_service::wait(event<> _handle, std::uint64_t _nano_seconds, thread_t _thread) noexcept
    {
        const std::uint64_t sleep_mark = current_ + _nano_seconds;

        bool change_rate = false;

        auto it = waiting_.find(sleep_mark);
        if (it == waiting_.end())
        {
            auto [_it_, _s_] = waiting_.emplace(
                sleep_mark,
                std::vector<std::pair<event<>, thread_t>>{{_handle, engine_->current_id()}});

            if (_it_ == waiting_.begin()) { change_rate = true; }
        }
        else
        {
            it->second.emplace_back(_handle, _thread);
        }

        if (change_rate) { change_timer(_nano_seconds); }
    }

}   // namespace zab