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
#include <mutex>
#include <sys/timerfd.h>

#include "zab/async_primitives.hpp"
#include "zab/descriptor_notifications.hpp"
#include "zab/engine.hpp"
#include "zab/event.hpp"

namespace zab {

    timer_service::timer_service(engine* _engine) : engine_(_engine), current_(0)
    {
        int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

        if (timer_fd == -1)
        {
            std::cerr << "timer_service -> Failed to create timerfd. errno:" << errno << "\n";
            abort();
        }

        auto waiter_opt = engine_->get_notification_handler().subscribe(timer_fd);

        if (!waiter_opt)
        {
            std::cerr << "timer_service -> Failed to subscribe to timerfd. errno:" << errno << "\n";
            abort();
        }

        swap(waiter_, *waiter_opt);

        run();
    }

    timer_service::~timer_service()
    {
        auto desc = waiter_.file_descriptor();
        if (desc)
        {
            if (close(desc))
            {
                std::cerr << "timer_service -> Failed to close timer during deconstruction. errno:"
                          << errno << "\n";
                abort();
            }
        }
    }

    async_function<>
    timer_service::run()
    {
        co_await yield(engine_, order_t{0}, thread_t{0});

        struct itimerspec current_spec;
        waiter_.set_flags(descriptor_notification::kRead);
        while (true)
        {
            std::cout << "We are waiting!";
            auto flags = co_await waiter_;
            std::cout << "We are waking!!";
            if (flags | descriptor_notification::kRead)
            {
                std::lock_guard lck(mtx_);
                std::uint64_t   current_value = 0;

                int rc = ::read(waiter_.file_descriptor(), &current_value, sizeof(current_value));
                if (rc < 0 && errno != EAGAIN)
                {
                    std::cerr << "timer_service -> read failed. errno:" << errno << "\n";
                    abort();
                }

                if (current_value)
                {

                    rc = timerfd_gettime(waiter_.file_descriptor(), &current_spec);
                    if (rc < 0)
                    {
                        std::cerr << "timer_service -> timerfd_gettime failed. errno:" << errno
                                  << "\n";
                        abort();
                    }

                    current_ +=
                        ((((std::uint64_t) current_spec.it_interval.tv_sec) * kNanoInSeconds) +
                         current_spec.it_interval.tv_nsec) *
                        current_value;

                    for (auto it = waiting_.begin(); it != waiting_.end() && it->first <= current_;)
                    {
                        for (const auto& [handle, thread] : it->second)
                        {
                            engine_->resume(handle, order::now(), thread);
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
                            waiter_.file_descriptor(),
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
                }
            }
        }
    }

    void
    timer_service::wait(std::coroutine_handle<> _handle, std::uint64_t _nano_seconds) noexcept
    {
        wait(_handle, _nano_seconds, engine_->current_id());
    }

    void
    timer_service::wait(
        std::coroutine_handle<> _handle,
        std::uint64_t           _nano_seconds,
        thread_t                _thread) noexcept
    {
        std::cout << "We are going to wait for " << _nano_seconds << "\n";
        std::lock_guard     lck(mtx_);
        const std::uint64_t sleep_mark = current_ + _nano_seconds;

        bool change_rate = false;

        auto it = waiting_.find(sleep_mark);
        if (it == waiting_.end())
        {
            auto [_it_, _s_] = waiting_.emplace(
                sleep_mark,
                std::vector<std::pair<std::coroutine_handle<>, thread_t>>{
                    {_handle, engine_->current_id()}});

            if (_it_ == waiting_.begin()) { change_rate = true; }
        }
        else
        {
            it->second.emplace_back(_handle, _thread);
        }

        if (change_rate)
        {
            std::cout << "Setting timer spec! ";
            struct itimerspec new_value;

            new_value.it_value.tv_sec  = _nano_seconds / kNanoInSeconds;
            new_value.it_value.tv_nsec = _nano_seconds % kNanoInSeconds;

            new_value.it_interval = new_value.it_value;

            auto rc = timerfd_settime(
                waiter_.file_descriptor(),
                0, /* relative */
                &new_value,
                nullptr);

            if (rc < 0)
            {
                std::cerr << "timer_service -> timerfd_settime failed (2). errno:" << errno << "\n";
                abort();
            }
        }
    }

}   // namespace zab