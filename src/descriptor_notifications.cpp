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
 *  @file descriptor_notifications.cpp
 *
 */

#include "zab/descriptor_notifications.hpp"

#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <unordered_set>
#include <utility>

#include "zab/engine.hpp"

namespace zab {

    descriptor_notification::descriptor_notification(engine* _engine)
        : awaiting_mtx_(std::make_unique<std::mutex>()), engine_(_engine)
    {

        event_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (event_fd_ == -1)
        {
            std::cerr << "descriptor_notification -> Failed to create eventfd. errno:" << errno
                      << "\n";
            abort();
        }

        poll_descriptor_ = epoll_create1(EPOLL_CLOEXEC);
        if (poll_descriptor_ < 0)
        {
            std::cerr << "descriptor_notification -> Failed to create epoll. errno:" << errno
                      << "\n";
            abort();
        }

        struct epoll_event events {
                .events = kRead | kException, .data = epoll_data { .u32 = 1 }
        };

        epoll_ctl(poll_descriptor_, EPOLL_CTL_ADD, event_fd_, &events);
    }

    descriptor_notification::~descriptor_notification()
    {
        if (close(event_fd_) || close(poll_descriptor_))
        {
            std::cerr << "Failed to close Notifier pipe or epoll during deconstruction. errno:"
                      << errno << "\n";
            abort();
        }

        epoll_ctl(poll_descriptor_, EPOLL_CTL_DEL, event_fd_, nullptr);

        if (!notification_loop_.get_stop_token().stop_requested())
        {
            notification_loop_.request_stop();
        }

        if (notification_loop_.joinable()) { notification_loop_.join(); }

        /* Manually resume all awaiting in order for them to perform some clean up */
        /* before the system shuts down... */
        for (auto& i : awaiting_)
        {
            i->return_flags_ = kDestruction;
            if (auto handle = i->awaiter_.exchange(nullptr, std::memory_order_release); handle)
            {
                std::coroutine_handle<>::from_address(handle).resume();
            }
        }
    }

    auto
    descriptor_notification::subscribe(int _fd) -> std::optional<descriptor_waiter>
    {
        std::scoped_lock lck(*awaiting_mtx_);

        awaiting_.emplace_back(std::make_unique<descriptor>());

        auto* desc = awaiting_.back().get();

        struct epoll_event events {
                .events = 0, .data = epoll_data { .ptr = desc }
        };

        auto rc = epoll_ctl(poll_descriptor_, EPOLL_CTL_ADD, _fd, &events);

        if (rc < 0) [[unlikely]]
        {
            awaiting_.pop_back();
            return std::nullopt;
        }

        return descriptor_waiter(this, desc, _fd);
    }

    descriptor_notification::descriptor::descriptor()
        : awaiter_(nullptr), return_flags_(0), timeout_(-1), dead_(0)
    { }

    descriptor_notification::descriptor_waiter::descriptor_waiter(
        descriptor_notification* _self,
        descriptor*              _desc,
        int                      _fd)
        : self_(_self), desc_(_desc), fd_(_fd), timeout_(0)
    { }

    descriptor_notification::descriptor_waiter::descriptor_waiter()
        : self_(nullptr), desc_(nullptr), fd_(-1)
    { }

    descriptor_notification::descriptor_waiter::descriptor_waiter(descriptor_waiter&& _move)
        : descriptor_waiter()
    {
        swap(*this, _move);
    }

    auto
    descriptor_notification::descriptor_waiter::operator=(descriptor_waiter&& _move)
        -> descriptor_waiter&
    {
        swap(*this, _move);
        return *this;
    }

    void
    swap(
        descriptor_notification::descriptor_waiter& _first,
        descriptor_notification::descriptor_waiter& _second) noexcept
    {
        using std::swap;
        swap(_first.self_, _second.self_);
        swap(_first.desc_, _second.desc_);
        swap(_first.fd_, _second.fd_);
    }

    descriptor_notification::descriptor_waiter::~descriptor_waiter()
    {
        if (desc_)
        {
            auto desc = self_->poll_descriptor_;
            epoll_ctl(desc, EPOLL_CTL_DEL, fd_, nullptr);
            desc_->dead_.store(true, std::memory_order_acquire);
        }
    }

    void
    descriptor_notification::descriptor_waiter::wake_up() noexcept
    {
        std::scoped_lock lck(*self_->awaiting_mtx_);
        desc_->set_timeout(0);
        std::uint64_t wake_up = 1;
        if (int error = ::write(self_->event_fd_, &wake_up, sizeof(wake_up)); error < 0)
        {
            std::cerr << "Failed to write to event_fd_. errno:" << errno << "\n";
        }
    }

    void
    descriptor_notification::descriptor_waiter::await_proxy::await_suspend(
        std::coroutine_handle<> _awaiter) noexcept
    {
        this_->desc_->set_handle(_awaiter);
        struct epoll_event events {
                .events = this_->flags_ | EPOLLONESHOT, .data = epoll_data { .ptr = this_->desc_ }
        };

        this_->desc_->set_timeout(this_->timeout_);
        epoll_ctl(this_->self_->poll_descriptor_, EPOLL_CTL_MOD, this_->fd_, &events);

        if (this_->timeout_)
        {
            std::uint64_t wake_up = 1;
            if (int error = ::write(this_->self_->event_fd_, &wake_up, sizeof(wake_up)); error < 0)
            {
                std::cerr << "Failed to write to event_fd_. errno:" << errno << "\n";
            }
        }
    }

    void
    descriptor_notification::descriptor::set_handle(std::coroutine_handle<> _handle) noexcept
    {
        awaiter_.store(_handle.address(), std::memory_order_acquire);
    }

    void
    descriptor_notification::notify(descriptor* _awaiting, int _flags)
    {
        _awaiting->return_flags_ = _flags;
        if (auto handle = _awaiting->awaiter_.exchange(nullptr, std::memory_order_release); handle)
        {
            engine_->resume(
                coroutine{std::coroutine_handle<>::from_address(handle)},
                order_t{order::now()},
                _awaiting->thread_);
        }
    }

    void
    descriptor_notification::run()
    {
        notification_loop_ = std::jthread(
            [this](std::stop_token _stop_token)
            {
                std::stop_callback callback(
                    _stop_token,
                    [this]
                    {
                        std::uint64_t wake_up = 1;
                        if (int error = ::write(event_fd_, &wake_up, sizeof(wake_up)); error < 0)
                        {
                            std::cerr << "Failed to write to event_fd_. errno:" << errno << "\n";
                        }
                    });

                const int max_events = 1028;

                struct epoll_event events[max_events];
                while (!_stop_token.stop_requested())
                {
                    int  timeout    = -1;
                    auto start_time = std::chrono::steady_clock::now();

                    { /* Find the timeout if one */
                        std::scoped_lock lck(*awaiting_mtx_);
                        for (const auto& a : awaiting_)
                        {
                            if (a->timeout_ >= 0 && (timeout == -1 || a->timeout_ < timeout))
                            {
                                timeout = a->timeout_;
                            }
                        }
                    }

                    int rc = ::epoll_wait(poll_descriptor_, events, max_events, timeout);

                    auto stop_time = std::chrono::steady_clock::now();
                    auto duration =
                        duration_cast<std::chrono::milliseconds>(stop_time - start_time);

                    if (rc < 0 && errno != EINTR) [[unlikely]] { abort(); }

                    if (_stop_token.stop_requested()) { break; }

                    std::unordered_set<descriptor*> seen;
                    for (int i = 0; i < rc; ++i)
                    {
                        if (events[i].data.u32 > 1)
                        {
                            descriptor* awaiting =
                                reinterpret_cast<descriptor*>(events[i].data.ptr);
                            notify(awaiting, events[i].events);
                            seen.emplace(awaiting);
                        }
                        else if (events[i].data.u32 == 1)
                        {
                            /* Clear the event fd*/
                            std::uint64_t wake_up = 0;
                            if (int error = ::read(event_fd_, &wake_up, sizeof(wake_up)); error < 0)
                            {
                                std::cerr << "Failed to read from event_fd_. errno:" << errno
                                          << "\n";
                            }
                        }
                    }

                    if (timeout >= 0)
                    {
                        auto             amount_passed = duration.count();
                        std::scoped_lock lck(*awaiting_mtx_);
                        for (auto it = awaiting_.begin(); it != awaiting_.end();)
                        {
                            if ((*it)->dead_.load(std::memory_order_release))
                            {
                                it = awaiting_.erase(it);
                            }
                            else
                            {
                                if ((*it)->timeout_ >= 0)
                                {
                                    (*it)->timeout_ -= amount_passed + 1;

                                    if ((*it)->timeout_ < 0 && !seen.contains(it->get()))
                                    {
                                        notify(it->get(), 0);
                                    }
                                }

                                ++it;
                            }
                        }
                    }
                }
            });
    }

    void
    descriptor_notification::stop()
    {
        notification_loop_.request_stop();
        if (notification_loop_.joinable()) { notification_loop_.join(); }
    }

}   // namespace zab