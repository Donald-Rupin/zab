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
#include <memory>
#include <mutex>
#include <optional>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <unordered_set>
#include <utility>

#include "zab/engine.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    descriptor_notification::descriptor_notification(engine* _engine) : engine_(_engine)
    {
        event_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
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
            ::abort();
        }

        struct epoll_event e_events {
                .events = kRead | kException, .data = epoll_data { .fd = event_fd_ }
        };

        ::epoll_ctl(poll_descriptor_, EPOLL_CTL_ADD, event_fd_, &e_events);
    }

    descriptor_notification::~descriptor_notification()
    {
        if (!notification_loop_.get_stop_token().stop_requested())
        {
            notification_loop_.request_stop();
        }

        if (notification_loop_.joinable()) { notification_loop_.join(); }

        ::epoll_ctl(poll_descriptor_, EPOLL_CTL_DEL, event_fd_, nullptr);

        if (close(event_fd_) || close(poll_descriptor_))
        {
            std::cerr << "Failed to close event or epoll during deconstruction. errno:" << errno
                      << "\n";
            abort();
        }

        for (const auto d : pending_action_.decs_)
        {
            in_action_.emplace(d);
        }
        pending_action_.decs_.clear();

        for (const auto d : for_deletion_.decs_)
        {
            in_action_.emplace(d);
        }
        for_deletion_.decs_.clear();

        /* delete those in action */
        for (const auto i : in_action_)
        {
            destroy(i);
        }
    }

    void
    descriptor_notification::destroy(descriptor* _to_delete) const noexcept
    {
        auto handle = _to_delete->awaiter_.exchange(nullptr);
        if (handle) { handle->handle_.destroy(); }

        delete _to_delete;
    }

    auto
    descriptor_notification::subscribe(int _fd) noexcept -> std::optional<descriptor_waiter>
    {

        auto desc = new descriptor;

        struct epoll_event events {
                .events = 0, .data = epoll_data { .ptr = desc }
        };

        auto rc = epoll_ctl(poll_descriptor_, EPOLL_CTL_ADD, _fd, &events);

        if (rc < 0) [[unlikely]]
        {
            delete desc;
            return std::nullopt;
        }

        {
            std::scoped_lock lck(pending_action_.mtx_);
            pending_action_.decs_.push_back(desc);
        }

        return descriptor_waiter(this, desc, _fd);
    }

    descriptor_notification::descriptor::descriptor() : awaiter_(nullptr) { }

    descriptor_notification::descriptor_waiter::descriptor_waiter(
        descriptor_notification* _self,
        descriptor*              _desc,
        int                      _fd)
        : self_(_self), desc_(_desc), fd_(_fd)
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
            self_->notify(desc_, 0);

            epoll_ctl(self_->poll_descriptor_, EPOLL_CTL_DEL, fd_, nullptr);

            {
                std::scoped_lock lck(self_->for_deletion_.mtx_);
                self_->for_deletion_.decs_.push_back(desc_);
            }
        }
    }

    auto descriptor_notification::descriptor_waiter::operator co_await() noexcept -> await_proxy
    {
        return await_proxy{
            .self_         = this,
            .handle_       = nullptr,
            .return_flags_ = 0,
            .thread_       = self_->engine_->current_id()};
    }

    void
    descriptor_notification::descriptor_waiter::await_proxy::await_suspend(
        std::coroutine_handle<> _awaiter) noexcept
    {
        handle_ = _awaiter;

        self_->desc_->set_handle(self_->self_->engine_, this);

        struct epoll_event events {
                .events = self_->flags_ | EPOLLONESHOT, .data = epoll_data { .ptr = self_->desc_ }
        };

        epoll_ctl(self_->self_->poll_descriptor_, EPOLL_CTL_MOD, self_->fd_, &events);
    }

    void
    descriptor_notification::descriptor::set_handle(
        engine*                         _engine,
        descriptor_waiter::await_proxy* _handle) noexcept
    {

        auto tmp = awaiter_.exchange(_handle, std::memory_order_acquire);

        if (tmp)
        {
            tmp->return_flags_ = 0;
            _engine->resume(tmp->handle_, order_t{order::now()}, tmp->thread_);
        }
    }

    void
    descriptor_notification::notify(descriptor* _awaiting, int _flags) noexcept
    {
        if (auto handle = _awaiting->awaiter_.exchange(nullptr, std::memory_order_release); handle)
        {
            handle->return_flags_ = _flags;
            auto tmp              = handle->handle_;
            handle->handle_       = nullptr;
            engine_->resume(tmp, order_t{order::now()}, handle->thread_);
        }
    }

    void
    descriptor_notification::run() noexcept
    {
        notification_loop_ = std::jthread([this](std::stop_token _stop_token) noexcept
                                          { notification_loop(std::move(_stop_token)); });
    }

    void
    descriptor_notification::notification_loop(std::stop_token _stop_token) noexcept
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
            int rc = ::epoll_wait(poll_descriptor_, events, max_events, -1);

            if (rc < 0 && errno != EINTR) [[unlikely]] { abort(); }

            if (_stop_token.stop_requested()) { break; }

            bool do_event = false;
            for (int i = 0; i < rc; ++i)
            {
                if (events[i].data.fd == event_fd_) { do_event = true; }
                else
                {
                    descriptor* awaiting = reinterpret_cast<descriptor*>(events[i].data.ptr);

                    notify(awaiting, events[i].events);
                }
            }

            if (do_event) { handle_event(); }
        }
    }

    void
    descriptor_notification::handle_event() noexcept
    {
        /* Clear the event fd*/
        std::uint64_t wake_up = 0;
        if (int error = ::read(event_fd_, &wake_up, sizeof(wake_up)); error < 0)
        {
            std::cerr << "Failed to read from event_fd_. errno:" << errno << "\n";
        }

        decltype(pending_action_.decs_) tmp_pending;
        {
            std::scoped_lock lck(pending_action_.mtx_);
            tmp_pending.swap(pending_action_.decs_);
            pending_action_.decs_.clear();
        }

        /* Add new ones to set */
        for (const auto d : tmp_pending)
        {
            in_action_.emplace(d);
        }

        decltype(for_deletion_.decs_) re_insert;
        decltype(for_deletion_.decs_) to_delete;
        {
            std::scoped_lock lck(for_deletion_.mtx_);
            to_delete.swap(for_deletion_.decs_);
            for_deletion_.decs_.clear();
        }

        for (const auto d : to_delete)
        {
            if (auto it = in_action_.find(d); it != in_action_.end())
            {
                in_action_.erase(it);

                destroy(d);
            }
            else
            {
                /* Process again later */
                re_insert.push_back(d);
            }
        }

        if (re_insert.size())
        {

            std::scoped_lock lck(for_deletion_.mtx_);

            if (!for_deletion_.decs_.size()) { for_deletion_.decs_.swap(re_insert); }
            else
            {
                for (const auto d : re_insert)
                {
                    for_deletion_.decs_.push_back(d);
                }
            }
        }
    }

    void
    descriptor_notification::stop() noexcept
    {
        notification_loop_.request_stop();
        if (notification_loop_.joinable()) { notification_loop_.join(); }
    }

}   // namespace zab