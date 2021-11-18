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
 *
 *  @file binary_semaphore.hpp
 *
 */

#ifndef ZAB_BINARY_SEMAPHORE_HPP
#define ZAB_BINARY_SEMAPHORE_HPP

#include <atomic>
#include <coroutine>
#include <cstddef>

#include "zab/async_function.hpp"
#include "zab/async_primitives.hpp"
#include "zab/engine.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    template <std::ptrdiff_t Count = 2>
    class async_counting_semaphore {

        public:

            struct waiter {

                    std::coroutine_handle<>
                    await_suspend(std::coroutine_handle<> _awaiter) noexcept
                    {
                        auto result = semaphore_.active_count_.load(std::memory_order_acquire);

                        while (result > 0)
                        {

                            if (semaphore_.active_count_.compare_exchange_weak(
                                    result,
                                    result - 1,
                                    std::memory_order_acquire,
                                    std::memory_order_relaxed))
                            {

                                return _awaiter;
                            }
                        }

                        semaphore_.active_count_.fetch_sub(1, std::memory_order_acquire);

                        handle_ = _awaiter;

                        semaphore_.suspend(this);

                        return std::noop_coroutine();
                    }

                    bool
                    await_ready() const noexcept
                    {
                        return false;
                    }

                    void
                    await_resume() const noexcept
                    { }

                    async_counting_semaphore& semaphore_;
                    thread_t                  thread_       = thread_t{};
                    waiter*                   next_waiting_ = nullptr;
                    std::coroutine_handle<>   handle_       = nullptr;
            };

            async_counting_semaphore(engine* _engine, std::ptrdiff_t _start)
                : engine_(_engine), active_count_(_start)
            { }

            async_counting_semaphore(engine* _engine) : engine_(_engine), active_count_(Count) { }

            ~async_counting_semaphore() = default;

            bool
            try_aquire() noexcept
            {
                auto result = active_count_.load(std::memory_order_acquire);

                while (result > 0)
                {

                    if (active_count_.compare_exchange_weak(
                            result,
                            result - 1,
                            std::memory_order_acquire,
                            std::memory_order_relaxed))
                    {

                        break;
                    }
                }

                return result > 0;
            }

            void
            release(std::ptrdiff_t _update = 1) noexcept
            {
                auto count = release_count_.fetch_add(_update, std::memory_order_acquire);

                /* Do we need to enable the control thread */
                /* To start the resuming process           */
                if (!count) { control(); }
            }

            waiter operator co_await() noexcept
            {
                return {*this, engine_->get_event_loop().current_id()};
            }

        private:

            async_function<>
            control()
            {
                auto   thread = engine_->get_event_loop().current_id();
                size_t count  = 0;

                while (true)
                {

                    auto resume = transfer();

                    if (!resume)
                    {

                        auto result = active_count_.fetch_add(1, std::memory_order_release);

                        if (result < 0)
                        {

                            while (!resume)
                            {

                                /* Someone must of counted down  */
                                /* but not suspended...          */
                                co_await yield(engine_, order_t{order::now()}, thread);

                                resume = transfer();
                            }

                            /* resume them */
                            engine_->resume(
                                resume->handle_,
                                order_t{order::now()},
                                resume->thread_);
                        }

                        auto waiting = release_count_.fetch_sub(1, std::memory_order_release);

                        if (waiting == 1) { co_return; }

                        ++count;
                    }
                    else
                    {

                        active_count_.fetch_add(1, std::memory_order_release);

                        /* resume them */
                        engine_->resume(resume->handle_, order_t{order::now()}, resume->thread_);

                        auto waiting = release_count_.fetch_sub(1, std::memory_order_release);

                        /* No more to release... */
                        if (waiting == 1) { co_return; }

                        ++count;
                    }

                    /* Every 64 iterations yield back*/
                    if (count & 0b1000000)
                    {

                        co_await yield(engine_, order_t{order::now()}, thread);
                    }
                }
            }

            void
            suspend(waiter* _waiter)
            {
                /* Unlike binary semaphore, if we get here we must add our self to the list */
                /* There may be thread spinning waiting for us to be added.... */
                auto previous = resuming_.load(std::memory_order_acquire);
                do
                {

                    _waiter->next_waiting_ = previous;

                } while (!resuming_.compare_exchange_weak(
                    previous,
                    _waiter,
                    std::memory_order_release,
                    std::memory_order_relaxed));
            }

            waiter*
            transfer() noexcept
            {
                waiter* head = transfer_;

                if (!head)
                {

                    /* Must be a waiter out there */
                    auto previous = resuming_.exchange(0, std::memory_order_acq_rel);

                    if (!previous) { return nullptr; }

                    /* Reverse the waiting handles and transfer them the mutex for processing*/
                    waiter* next = reinterpret_cast<waiter*>(previous);
                    waiter* temp = nullptr;
                    do
                    {
                        temp                = next->next_waiting_;
                        next->next_waiting_ = head;
                        head                = next;
                        next                = temp;

                    } while (next);

                    transfer_ = head;
                }

                transfer_ = head->next_waiting_;

                return head;
            }

            friend struct waiter;

            engine* engine_;

            std::atomic<waiter*> resuming_ = nullptr;

            std::atomic<std::ptrdiff_t> active_count_ = Count;

            std::atomic<std::ptrdiff_t> release_count_ = 0;

            waiter* transfer_ = nullptr;
    };

    template <>
    class async_counting_semaphore<1> {

        public:

            struct waiter {

                    std::coroutine_handle<>
                    await_suspend(std::coroutine_handle<> _awaiter) noexcept
                    {
                        handle_ = _awaiter;

                        if (semaphore_.suspend(this)) { return std::noop_coroutine(); }
                        else
                        {

                            return _awaiter;
                        }
                    }

                    bool
                    await_ready() const noexcept
                    {
                        return semaphore_.try_aquire();
                    }

                    void
                    await_resume() const noexcept
                    { }

                    async_counting_semaphore& semaphore_;
                    thread_t                  thread_       = thread_t{};
                    waiter*                   next_waiting_ = nullptr;
                    std::coroutine_handle<>   handle_       = nullptr;
            };

            async_counting_semaphore(engine* _engine, bool _unlocked)
                : engine_(_engine), resuming_(_unlocked ? 0 : kLockFlag)
            { }

            ~async_counting_semaphore() = default;

            bool
            try_aquire() noexcept
            {
                std::uintptr_t not_locked = 0;
                return resuming_.compare_exchange_strong(
                    not_locked,
                    kLockFlag,
                    std::memory_order_acquire,
                    std::memory_order_relaxed);
            }

            void
            release() noexcept
            {
                waiter* head = transfer_;

                if (!head)
                {

                    /* Is the state just locked? */
                    std::uintptr_t previous = kLockFlag;
                    if (resuming_.compare_exchange_strong(
                            previous,
                            0,
                            std::memory_order_release,
                            std::memory_order_relaxed))
                    {

                        return;
                    }

                    /* Must be a waiter out there */
                    previous = resuming_.exchange(kLockFlag, std::memory_order_acquire);

                    /* Reverse the waiting handles and transfer them the mutex for processing*/
                    waiter* next = reinterpret_cast<waiter*>(previous);
                    waiter* temp = nullptr;
                    do
                    {
                        temp                = next->next_waiting_;
                        next->next_waiting_ = head;
                        head                = next;
                        next                = temp;

                    } while (next && next != reinterpret_cast<waiter*>(kLockFlag));
                }

                /* Either we have a list to process, or we just got one */
                transfer_ = head->next_waiting_;

                if (head->handle_)
                {

                    engine_->resume(head->handle_, order_t{order::now()}, head->thread_);
                }
            }

            /**
             * @brief      Acquire()
             *
             * @return     Acquires the semaphore.
             *
             */
            waiter operator co_await() noexcept
            {
                return {*this, engine_->get_event_loop().current_id()};
            }

        private:

            static constexpr std::uintptr_t kLockFlag = 0b1;

            friend struct waiter;

            bool
            suspend(waiter* _waiter)
            {
                /* If we get here... it was locked when we came */
                /* This now may not be the case...              */
                std::uintptr_t previous = resuming_.load(std::memory_order_acquire);
                do
                {

                    if (!previous)
                    {

                        if (resuming_.compare_exchange_weak(
                                previous,
                                kLockFlag,
                                std::memory_order_acquire,
                                std::memory_order_relaxed))
                        {

                            return false;
                        }
                    }
                    else
                    {

                        _waiter->next_waiting_ = reinterpret_cast<waiter*>(previous);
                        if (resuming_.compare_exchange_weak(
                                previous,
                                reinterpret_cast<std::uintptr_t>(_waiter),
                                std::memory_order_release,
                                std::memory_order_relaxed))
                        {

                            return true;
                        }
                    }

                } while (true);
            }

            engine* engine_;

            std::atomic<std::uintptr_t> resuming_ = 0;
            waiter*                     transfer_ = nullptr;
    };

    using async_binary_semaphore = async_counting_semaphore<1>;

}   // namespace zab

#endif /* ZAB_BINARY_SEMAPHORE_HPP */