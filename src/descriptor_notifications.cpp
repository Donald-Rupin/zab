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
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <unordered_set>
#include <utility>

#include "zab/engine.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    descriptor_notification::descriptor_notification(engine* _engine)
        : timers_(nullptr), engine_(_engine)
    {

        event_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (event_fd_ == -1)
        {
            std::cerr << "descriptor_notification -> Failed to create eventfd. errno:" << errno
                      << "\n";
            abort();
        }

        timer_fd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);

        if (timer_fd_ < 0)
        {
            std::cerr << "descriptor_notification -> Failed to create timerfd. errno:" << errno
                      << "\n";
            ::abort();
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

        struct epoll_event t_events {
                .events = kRead | kException, .data = epoll_data { .fd = timer_fd_ }
        };

        ::epoll_ctl(poll_descriptor_, EPOLL_CTL_ADD, event_fd_, &t_events);
    }

    descriptor_notification::~descriptor_notification()
    {
        if (!notification_loop_.get_stop_token().stop_requested())
        {
            notification_loop_.request_stop();
        }

        if (notification_loop_.joinable()) { notification_loop_.join(); }

        ::epoll_ctl(poll_descriptor_, EPOLL_CTL_DEL, event_fd_, nullptr);
        ::epoll_ctl(poll_descriptor_, EPOLL_CTL_DEL, timer_fd_, nullptr);

        if (close(event_fd_) || close(timer_fd_) || close(poll_descriptor_))
        {
            std::cerr << "Failed to close event, timer or epoll during deconstruction. errno:"
                      << errno << "\n";
            abort();
        }

        /* delete waiting coroutines */
        auto tmp = awaiting_timer_.load();
    
        while (tmp)
        {
            auto next = tmp->next_;

            if (!(tmp->context_ & kDecriptorFlag))
            {
                waiting_coroutine* coro =
                    reinterpret_cast<waiting_coroutine*>(tmp->context_ & kAddressMask);

                coro->handle_.destroy();
            }

            tmp = next;

            if (!tmp) {
                tmp = timers_;
            }   
        }

        /* delete those in action */
        process_pending_actions();
        for (const auto i : in_action_)
        {
            auto handle = i->awaiter_.load();
            if (handle) {
                std::coroutine_handle<>::from_address(handle).destroy();
            }
            
            delete i;
        }
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

        add_in_action(desc);

        return descriptor_waiter(this, desc, _fd);
    }

    descriptor_notification::descriptor::descriptor()
        : awaiter_(nullptr), dead_chain_(nullptr), live_chain_(nullptr), return_flags_(0)
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
            epoll_ctl(self_->poll_descriptor_, EPOLL_CTL_DEL, fd_, nullptr);
            self_->add_deleted(desc_);
        }
    }

    void
    descriptor_notification::descriptor_waiter::wake_up() noexcept
    {
        self_->add_wakeup(desc_);

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

        if (this_->timeout_ != -1)
        {

            auto time             = this_->desc_->get_timeout();
            time->time_to_expiry_ = this_->timeout_;
            time->context_        = reinterpret_cast<std::uintptr_t>(this_->desc_) | kDecriptorFlag;
            this_->self_->add_timer(time);
        }

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
    descriptor_notification::notify(descriptor* _awaiting, int _flags) noexcept
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

            bool do_timer = false;
            bool do_event = false;
            for (int i = 0; i < rc; ++i)
            {
                if (events[i].data.fd == timer_fd_) { do_timer = true; }
                else if (events[i].data.fd == event_fd_)
                {
                    do_event = true;
                }
                else
                {
                    descriptor* awaiting = reinterpret_cast<descriptor*>(events[i].data.ptr);

                    if (awaiting->timeout_.time_to_expiry_ != -1)
                    {
                        /* Need to remove from timer list */
                        auto tmp = timers_;
                        if (tmp == &awaiting->timeout_) { timers_ = tmp->next_; }
                        else
                        {
                            do
                            {
                                while (tmp && tmp->next_ != &awaiting->timeout_)
                                {
                                    tmp = tmp->next_;
                                }

                                if (tmp) { tmp->next_ = tmp->next_->next_; }
                                else
                                {
                                    transfer_timers(false);
                                }
                            } while (tmp);
                        }

                        awaiting->timeout_.time_to_expiry_ = -1;
                    }

                    notify(awaiting, events[i].events);
                }
            }

            if (do_timer) { handle_timers(); }

            if (do_event) { handle_event(); }
        }
    }

    void
    descriptor_notification::handle_timers() noexcept
    {
        std::uint64_t timers_emitted = 0;
        if (int error = ::read(timer_fd_, &timers_emitted, sizeof(timers_emitted));
            error < 0 && errno != EAGAIN)
        {
            std::cerr << "Failed to read from timer_fd_. errno:" << errno << "\n";
        }

        if (timers_emitted)
        {
            /* disarm the timer */
            struct itimerspec timer {
                    .it_interval = {0, 0}, .it_value = { 0, 0 }
            };
            struct itimerspec old_timer;

            ::timerfd_settime(timer_fd_, 0, &timer, &old_timer);

            std::int64_t time_elapsed =
                (old_timer.it_value.tv_nsec + (old_timer.it_value.tv_sec * kNanoInSeconds)) +
                ((old_timer.it_interval.tv_nsec + (old_timer.it_interval.tv_sec * kNanoInSeconds)) *
                 timers_emitted);

            auto tmp = timers_;
            while (tmp && tmp->time_to_expiry_ < time_elapsed)
            {
                if (tmp->context_ & kDecriptorFlag)
                {
                    descriptor* desc = reinterpret_cast<descriptor*>(tmp->context_ & kAddressMask);
                    tmp->time_to_expiry_ = -1;
                    notify(desc, 0);
                }
                else
                {
                    waiting_coroutine* coro =
                        reinterpret_cast<waiting_coroutine*>(tmp->context_ & kAddressMask);

                    engine_->resume(coroutine{coro->handle_}, order_t{}, coro->thread_);
                }
            }

            timers_ = tmp;
            while (tmp)
            {
                tmp->time_to_expiry_ -= time_elapsed;
                tmp = tmp->next_;
            }

            transfer_timers(true);
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

        transfer_timers(false);
        process_wakeups();
        process_pending_actions();
        process_deleted();
    }

    void
    descriptor_notification::add_timer(timer_event* _desc) noexcept
    {
        auto next = awaiting_timer_.load(std::memory_order_acquire);
        do
        {
            _desc->next_ = next;

        } while (awaiting_timer_.compare_exchange_weak(
            next,
            _desc,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void
    descriptor_notification::add_wakeup(descriptor* _desc) noexcept
    {
        auto next = for_wake_up_.load(std::memory_order_acquire);
        do
        {
            _desc->dead_chain_ = next;

        } while (for_wake_up_.compare_exchange_weak(
            next,
            _desc,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void
    descriptor_notification::add_deleted(descriptor* _desc) noexcept
    {
        auto next = for_deletion_.load(std::memory_order_acquire);
        do
        {
            _desc->dead_chain_ = next;

        } while (for_deletion_.compare_exchange_weak(
            next,
            _desc,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void
    descriptor_notification::process_deleted() noexcept
    {
        if (auto next_delete = for_deletion_.exchange(nullptr, std::memory_order_release))
        {
            while (next_delete)
            {
                descriptor* desc = reinterpret_cast<descriptor*>(next_delete);

                auto next = desc->dead_chain_;

                in_action_.erase(desc);

                delete desc;

                next_delete = next;
            }
        }
    }

    void
    descriptor_notification::add_in_action(descriptor* _desc) noexcept
    {
        auto next = pending_in_action_.load(std::memory_order_acquire);
        do
        {
            _desc->live_chain_ = next;

        } while (pending_in_action_.compare_exchange_weak(
            next,
            _desc,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    void
    descriptor_notification::process_pending_actions()
    {
        if (auto next_add = pending_in_action_.exchange(nullptr, std::memory_order_release))
        {
            while (next_add)
            {
                descriptor* desc = reinterpret_cast<descriptor*>(next_add);

                in_action_.emplace(desc);

                next_add = desc->live_chain_;
            }
        }
    }

    void
    descriptor_notification::process_wakeups() noexcept
    {
        if (auto to_wake_up = for_wake_up_.exchange(nullptr, std::memory_order_release))
        {
            while (to_wake_up)
            {
                descriptor* desc = reinterpret_cast<descriptor*>(to_wake_up);

                auto next         = desc->dead_chain_;
                desc->dead_chain_ = 0;

                if (desc->timeout_.time_to_expiry_ != -1)
                {
                    /* Need to remove from timer list */
                    auto tmp = timers_;
                    if (tmp == &desc->timeout_) { timers_ = tmp->next_; }
                    else
                    {
                        while (tmp && tmp->next_ != &desc->timeout_)
                        {
                            tmp = tmp->next_;
                        }

                        if (tmp) { tmp->next_ = tmp->next_->next_; }
                    }

                    desc->timeout_.time_to_expiry_ = -1;
                }

                notify(desc, 0);

                to_wake_up = next;
            }
        }
    }

    void
    descriptor_notification::transfer_timers(bool _always_update) noexcept
    {
        if (auto new_timers = awaiting_timer_.exchange(nullptr, std::memory_order_release))
        {
            _always_update = true;

            while (new_timers)
            {
                auto next         = new_timers->next_;
                new_timers->next_ = 0;

                if (!timers_)
                {
                    timers_    = new_timers;
                    new_timers = next;

                    if (new_timers)
                    {
                        next              = new_timers->next_;
                        new_timers->next_ = 0;
                    }
                    else
                    {

                        break;
                    }
                }

                /* It needs to go at the start! */
                while (new_timers && new_timers->time_to_expiry_ < timers_->time_to_expiry_)
                {

                    timers_    = new_timers;
                    new_timers = next;

                    if (new_timers)
                    {
                        next              = new_timers->next_;
                        new_timers->next_ = 0;
                    }
                }

                auto tmp = timers_;
                bool carry_on;
                do
                {
                    /* Iterate tmp until we find the position  */
                    while (new_timers && new_timers->time_to_expiry_ > tmp->time_to_expiry_)
                    {
                        if (tmp->next_) { tmp = tmp->next_; }
                        else
                        {
                            /* Insert at end */
                            tmp->next_ = new_timers;
                            new_timers = next;
                            break;
                        }
                    }

                    /* We didn't insert on the end */
                    if (new_timers != next)
                    {
                        /* Insert in middle */
                        new_timers->next_ = tmp->next_;
                        tmp->next_        = new_timers;
                        new_timers        = next;
                    }

                    /* Is the next one past the iterator position? */
                    if (new_timers && new_timers->time_to_expiry_ > tmp->time_to_expiry_)
                    {
                        /* if it is, we can keep on going and not reset the iterator tmp to the
                         * beginning */
                        next              = new_timers->next_;
                        new_timers->next_ = 0;
                        carry_on          = true;
                    }
                    else
                    {

                        carry_on = false;
                    }

                } while (carry_on);
            }
        }

        if (timers_ && _always_update)
        {
            struct itimerspec timer;
            timer.it_interval.tv_sec  = timers_->time_to_expiry_ / kNanoInSeconds;
            timer.it_interval.tv_nsec = timers_->time_to_expiry_ % kNanoInSeconds;
            timer.it_value            = timer.it_interval;

            ::timerfd_settime(timer_fd_, 0, &timer, nullptr);
        }
    }

    void
    descriptor_notification::stop() noexcept
    {
        notification_loop_.request_stop();
        if (notification_loop_.joinable()) { notification_loop_.join(); }
    }

}   // namespace zab