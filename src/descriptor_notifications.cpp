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

#include "zab/async_primitives.hpp"
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
        purge();
        if (poll_descriptor_)
        {
            ::epoll_ctl(poll_descriptor_, EPOLL_CTL_DEL, event_fd_, nullptr);

            if (close(event_fd_) || close(poll_descriptor_))
            {
                std::cerr << "Failed to close event or epoll during deconstruction. errno:" << errno
                          << "\n";
                abort();
            }

            poll_descriptor_ = 0;
            event_fd_        = 0;
        }
    }

    void
    descriptor_notification::purge()
    {
        if (!notification_loop_.get_stop_token().stop_requested())
        {
            notification_loop_.request_stop();
        }

        if (notification_loop_.joinable()) { notification_loop_.join(); }

        for (const auto d : pending_action_.decs_)
        {
            in_action_.emplace(d);
        }
        pending_action_.decs_.clear();

        for_deletion_.decs_.clear();

        /* delete those in action */
        for (const auto i : in_action_)
        {
            destroy(i);
        }
        in_action_.clear();
    }

    void
    descriptor_notification::destroy(descriptor* _to_delete) const noexcept
    {
        delete _to_delete;
    }

    auto
    descriptor_notification::subscribe(int _fd) noexcept -> std::optional<notifier>
    {
        auto desc = new descriptor(_fd, poll_descriptor_);

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

        return notifier(desc, this);
    }

    void
    descriptor_notification::remove(notifier& _notifier) noexcept
    {
        if (_notifier.desc_)
        {
            if (int error = ::epoll_ctl(
                    poll_descriptor_,
                    EPOLL_CTL_DEL,
                    _notifier.desc_->file_descriptor(),
                    nullptr);
                error < 0)
            {
                std::cerr << "Failed to EPOLL_CTL_DEL an fd. errno:" << errno << "\n";
            }

            {
                std::scoped_lock lck(for_deletion_.mtx_);
                for_deletion_.decs_.push_back(_notifier.desc_);
            }

            std::uint64_t wake_up = 1;
            if (int error = ::write(event_fd_, &wake_up, sizeof(wake_up)); error < 0)
            {
                std::cerr << "Failed to write to event_fd_. errno:" << errno << "\n";
            }

            _notifier.desc_ = nullptr;
        }
    }

    descriptor_notification::descriptor::descriptor(int _fd, int _poll)
        : ops_head_(nullptr), ops_tail_(nullptr), fd_(_fd), poll_descriptor_(_poll),
          flags_(
              descriptor_notification::kError | descriptor_notification::kException |
              descriptor_notification::kClosed)
    { }

    descriptor_notification::descriptor::~descriptor()
    {
        for (auto tmp = ops_head_; tmp;)
        {
            tmp->desc_ = nullptr;

            if (tmp->handle_)
            {
                tmp->flags_ = descriptor_notification::kError | descriptor_notification::kException;

                auto to_resume = tmp->handle_;
                tmp->handle_   = nullptr;
                tmp            = tmp->next_;

                to_resume.destroy();
            }
            else
            {
                tmp = tmp->next_;
            }
        }
    }

    descriptor_notification::notifier::notifier(descriptor* _desc, descriptor_notification* _notif)
        : desc_(_desc), notif_(_notif)
    { }

    descriptor_notification::notifier::notifier(notifier&& _move) : desc_(_move.desc_)
    {
        std::swap(desc_, _move.desc_);
        std::swap(notif_, _move.notif_);

        _move.desc_ = nullptr;
    }

    auto
    descriptor_notification::notifier::operator=(notifier&& _move) -> notifier&
    {
        remove();
        std::swap(desc_, _move.desc_);
        std::swap(notif_, _move.notif_);
        return *this;
    }

    descriptor_notification::notifier::~notifier() { remove(); }

    void
    descriptor_notification::notifier::remove()
    {
        if (desc_) { notif_->remove(*this); }
    }

    descriptor_notification::descriptor_op::descriptor_op(
        descriptor*             _desc,
        type                    _type,
        std::coroutine_handle<> _handle)
        : desc_(_desc), next_(nullptr), handle_(_handle), flags_(0), type_(_type)
    { }

    descriptor_notification::descriptor_op::~descriptor_op()
    {
        if (desc_) { desc_->remove(this); }

        if (handle_)
        {
            auto tmp = handle_;
            handle_  = nullptr;
            tmp.destroy();
        }
    }

    auto
    descriptor_notification::notifier::start_operation(descriptor_op::type _type) noexcept
        -> guaranteed_future<std::unique_ptr<descriptor_op>>
    {
        std::unique_ptr<descriptor_op> op;
        co_await zab::pause([this, &op, _type](auto* _pause_pack) noexcept
                            { desc_->add_operation(_type, _pause_pack->handle_, op); });

        co_return op;
    }

    void
    descriptor_notification::descriptor::add_operation(
        descriptor_op::type             _type,
        std::coroutine_handle<>         _handle,
        std::unique_ptr<descriptor_op>& _op) noexcept
    {
        bool reset_flags = false;
        {
            std::scoped_lock lck(mtx_);

            _op = std::make_unique<descriptor_notification::descriptor_op>(this, _type, _handle);
            if (ops_tail_)
            {
                ops_tail_->next_ = _op.get();
                ops_tail_        = _op.get();
            }
            else
            {
                ops_head_ = _op.get();
                ops_tail_ = _op.get();
            }

            reset_flags = re_arm_internal(_type);
        }

        if (reset_flags)
        {
            if (!set_epol())
            {
                /* The deconstructor will remove it */
                _op = nullptr;
            }
        }
    }

    bool
    descriptor_notification::descriptor::set_epol() noexcept
    {
        struct epoll_event events {
                .events = flags_ | EPOLLONESHOT, .data = epoll_data { .ptr = this }
        };

        return !epoll_ctl(poll_descriptor_, EPOLL_CTL_MOD, fd_, &events);
    }

    bool
    descriptor_notification::descriptor::re_arm(descriptor_op::type _type) noexcept
    {
        bool reset_flags = false;

        {
            std::scoped_lock lck(mtx_);
            reset_flags = re_arm_internal(_type);
        }

        if (reset_flags) { return set_epol(); }

        return true;
    }

    bool
    descriptor_notification::descriptor::re_arm_internal(descriptor_op::type _type) noexcept
    {
        bool reset_flags = false;

        if (_type == descriptor_op::type::kWrite)
        {
            if (!(flags_ & descriptor_notification::kWrite))
            {
                reset_flags = true;
                flags_ |= descriptor_notification::kWrite;
            }
        }
        else if (_type == descriptor_op::type::kRead)
        {
            if (!(flags_ & descriptor_notification::kRead))
            {
                reset_flags = true;
                flags_ |= descriptor_notification::kRead;
            }
        }
        else
        {
            if (!(flags_ & descriptor_notification::kWrite))
            {
                reset_flags = true;
                flags_ |= descriptor_notification::kWrite;
            }
            if (!(flags_ & descriptor_notification::kRead))
            {
                reset_flags = true;
                flags_ |= descriptor_notification::kRead;
            }
        }

        return reset_flags;
    }

    void
    descriptor_notification::descriptor::remove(descriptor_op* _to_remove)
    {
        std::scoped_lock lck(mtx_);

        if (_to_remove == ops_head_)
        {
            ops_head_ = ops_head_->next_;
            if (_to_remove == ops_tail_) { ops_tail_ = nullptr; }
        }
        else
        {
            auto prev = ops_head_;
            for (auto tmp = ops_head_->next_; tmp; tmp = tmp->next_)
            {
                if (tmp == _to_remove)
                {
                    prev->next_ = tmp->next_;
                    if (_to_remove == ops_tail_) { ops_tail_ = prev; }
                    break;
                }

                prev = tmp;
            }
        }
    }

    void
    descriptor_notification::descriptor::cancel()
    {
        std::scoped_lock lck(mtx_);

        for (auto tmp = ops_head_; tmp;)
        {
            auto to_cancel = tmp;

            tmp = tmp->next_;

            if (to_cancel->handle_)
            {
                to_cancel->flags_  = 0;
                auto to_resume     = to_cancel->handle_;
                to_cancel->handle_ = nullptr;
                to_resume.resume();
            }
        }
    }

    void
    descriptor_notification::notify(descriptor* _awaiting, int _flags) noexcept
    {
        descriptor_op*          read = nullptr;
        std::coroutine_handle<> read_handle_;

        descriptor_op*          write = nullptr;
        std::coroutine_handle<> write_handle_;

        {
            std::scoped_lock lck(_awaiting->mtx_);

            /* Can we successfully notify someone? */
            if (_flags & (descriptor_notification::kRead | descriptor_notification::kWrite))
            {
                for (auto tmp = _awaiting->ops_head_; tmp; tmp = tmp->next_)
                {
                    if (tmp->handle_)
                    {
                        if (!read && _flags & descriptor_notification::kRead)
                        {
                            if (tmp->type_ == descriptor_op::type::kRead ||
                                tmp->type_ == descriptor_op::type::kReadWrite)
                            {
                                read         = tmp;
                                read_handle_ = tmp->handle_;
                                tmp->handle_ = nullptr;
                            }
                        }

                        if (!write && _flags & descriptor_notification::kWrite)
                        {
                            if (tmp->type_ == descriptor_op::type::kWrite ||
                                tmp->type_ == descriptor_op::type::kReadWrite)
                            {
                                write         = tmp;
                                write_handle_ = tmp->handle_;
                                tmp->handle_  = nullptr;
                            }
                        }

                        if ((read || !(_flags & descriptor_notification::kRead)) &&
                            (write || !(_flags & descriptor_notification::kWrite)))
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                /* Just notify the first one of error */
                for (auto tmp = _awaiting->ops_head_; tmp; tmp = tmp->next_)
                {
                    if (tmp->handle_)
                    {
                        read         = tmp;
                        read_handle_ = tmp->handle_;
                        tmp->handle_ = nullptr;
                        break;
                    }
                }
            }
        }

        if (read)
        {
            /* Resume the waiter... */
            read->flags_ = _flags;
            read_handle_.resume();
        }

        if (read != write && write)
        {
            /* Resume the waiter... */
            write->flags_ = _flags;
            write_handle_.resume();
        }

        /* Recompute the flags... */
        {
            std::scoped_lock lck(_awaiting->mtx_);

            _awaiting->flags_ = descriptor_notification::kError |
                                descriptor_notification::kException |
                                descriptor_notification::kClosed;

            for (auto tmp = _awaiting->ops_head_; tmp; tmp = tmp->next_)
            {
                if (tmp->handle_)
                {
                    if (tmp->type_ == descriptor_op::type::kWrite)
                    {
                        _awaiting->flags_ |= descriptor_notification::kWrite;
                        if (_awaiting->flags_ & descriptor_notification::kRead) { break; }
                    }
                    else if (tmp->type_ == descriptor_op::type::kRead)
                    {
                        _awaiting->flags_ |= descriptor_notification::kRead;
                        if (_awaiting->flags_ & descriptor_notification::kWrite) { break; }
                    }
                    else
                    {
                        _awaiting->flags_ |=
                            descriptor_notification::kWrite | descriptor_notification::kRead;
                        break;
                    }
                }
            }
        }

        /* Reset if something is waiting */
        if (_awaiting->flags_ & (descriptor_notification::kWrite | descriptor_notification::kRead))
        {
            struct epoll_event events {
                    .events = _awaiting->flags_ | EPOLLONESHOT, .data = epoll_data
                    {
                        .ptr = _awaiting
                    }
            };

            if (epoll_ctl(poll_descriptor_, EPOLL_CTL_MOD, _awaiting->fd_, &events)) [[unlikely]]
            {
                for (auto tmp = _awaiting->ops_head_; tmp;)
                {
                    if (tmp->handle_)
                    {
                        tmp->flags_ =
                            descriptor_notification::kError | descriptor_notification::kException;

                        auto to_resume = tmp;
                        tmp            = tmp->next_;
                        to_resume->handle_.resume();
                    }
                    else
                    {
                        tmp = tmp->next_;
                    }
                }
            }
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
                    descriptor* awaiting = static_cast<descriptor*>(events[i].data.ptr);

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