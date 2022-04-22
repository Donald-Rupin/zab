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
 *  @file async_barrier.hpp
 *
 */

#ifndef ZAB_ASYNC_BARRIER_HPP_
#define ZAB_ASYNC_BARRIER_HPP_

#include <algorithm>
#include <atomic>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>

#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"
#include "zab/yield.hpp"

namespace zab {

    namespace details {

        /**
         * @brief      No-op phase completion step
         */
        struct no_op {

                /**
                 * @brief      Function call operator no-ops.
                 */
                void
                operator()() noexcept
                { }
        };

        /**
         * @brief      The object T is no-throw void(void)
         *             is no throw callable.
         *
         * @tparam     T     The object to use.
         *
         */
        template <typename T>
        concept NoThrowCallable = requires(T a)
        {
            {
                a()
            }
            noexcept->std::same_as<void>;
        };

        template <typename T>
        concept NoThrowAwaitable = requires(T a)
        {
            {
                a.operator co_await()
            }
            noexcept;
            {
                a.operator co_await().await_resume()
            }
            noexcept->std::same_as<void>;
        };

        template <typename T>
        concept Sequencer = NoThrowCallable<T> || NoThrowAwaitable<T>;

    }   // namespace details

    /**
     * @brief      This class describes an asynchronous barrier.
     *
     * @details    See: [std::barrier](https://en.cppreference.com/w/cpp/thread/barrier)
     *
     * @tparam     CompletionFunction  The phase completion step to
     *             execute during the phase complete step.
     *
     */
    template <details::Sequencer CompletionFunction = details::no_op>
    class async_barrier {

        public:

            /**
             * @brief      This class is the subscribeable proxy
             *             for suspending on the barrier phase.
             */
            class waiter {

                    friend class async_barrier;

                public:

                    ~waiter() = default;

                    waiter(const waiter& _copy) = delete;

                    waiter&
                    operator=(const waiter& _copy) = delete;

                    waiter&
                    operator=(waiter&& _copy) = delete;

                    void
                    await_suspend(std::coroutine_handle<> _awaiter) noexcept
                    {
                        handle_ = _awaiter;

                        auto amount = barrier_.count_.fetch_sub(1, std::memory_order_acquire);

                        barrier_.add_to_working_set(this);
                        if (amount == 1) { barrier_.complete_phase(); }
                    }

                    bool
                    await_ready() const noexcept
                    {
                        return false;
                    }

                    void
                    await_resume() const noexcept
                    { }

                private:

                    waiter(async_barrier& _barrier, thread_t _thread)
                        : barrier_(_barrier), thread_(_thread)
                    { }

                    waiter(waiter&& _move) = default;

                    async_barrier&          barrier_;
                    std::uintptr_t          next_;
                    std::coroutine_handle<> handle_;
                    thread_t                thread_;
            };

            /**
             * @brief      This class describes an arrival token used
             *             for arriving and suspending later.
             */
            class arrival_token {

                    friend class async_barrier;

                public:

                    ~arrival_token() = default;

                private:

                    arrival_token() : phase_complete_(std::make_unique<std::atomic<void*>>(nullptr))
                    { }

                    bool
                    await_suspend(std::coroutine_handle<> _awaiter) noexcept
                    {
                        void* expected = nullptr;
                        return !phase_complete_->handle_.compare_exchange_strong(
                            expected,
                            _awaiter.address(),
                            std::memory_order_release,
                            std::memory_order_relaxed);
                    }

                    bool
                    await_ready() const noexcept
                    {
                        return phase_complete_->handle_.load(std::memory_order_acquire) &
                               async_barrier::kFlagMask;
                    }

                    void
                    await_resume() const noexcept
                    { }

                    struct InternalState {

                            thread_t thread_;

                            std::uintptr_t next_;

                            std::atomic<void*> handle_;
                    };

                    std::unique_ptr<InternalState> phase_complete_;
            };

            /**
             * @brief      Construct the async_barrier
             *
             * @param[in]  _engine    The engine to run in
             * @param[in]  _expected  The number of threads to suspend
             * @param[in]  _function  The completion phase function
             * @param[in]  _thread    The control thread
             *
             */
            async_barrier(
                engine*                   _engine,
                std::ptrdiff_t            _expected,
                const CompletionFunction& _function = CompletionFunction{},
                thread_t                  _thread   = thread_t{})
                : engine_(_engine), expected_(_expected), count_(_expected), function_(_function),
                  thread_(_thread)
            { }

            async_barrier(
                engine*              _engine,
                std::ptrdiff_t       _expected,
                CompletionFunction&& _function = CompletionFunction{},
                thread_t             _thread   = thread_t{})
                : engine_(_engine), expected_(_expected), count_(_expected),
                  function_(std::move(_function)), thread_(_thread)
            { }

            async_barrier(const async_barrier&) = delete;

            async_barrier(async_barrier&&) = delete;

            /**
             * @brief      Default destroys the object.
             */
            ~async_barrier() = default;

            /**
             * @brief      arrive at the barrier but do not suspend.
             *
             * @return     The arrival token to await on later.
             *
             */
            [[nodiscard]] arrival_token

            arrive() noexcept
            {
                auto amount = count_.fetch_sub(1, std::memory_order_acquire);

                arrival_token token;

                add_to_working_set(token->phase_complete_.get());
                if (amount == 1) { complete_phase(); }

                return token;
            }

            /**
             * @brief      arrive at the barrier and suspend.
             *
             * @return     The waiter to co_await.
             *
             */
            [[nodiscard]] waiter

            arrive_and_wait() noexcept
            {
                return waiter(*this, engine_->current_id());
            }

            /**
             * @brief      arrive at the barrier and decrement
             *             the expected thread count after the
             *             current phase is complete.
             *
             */
            void
            arrive_and_drop() noexcept
            {
                auto amount = count_.fetch_sub(1, std::memory_order_acquire);

                add_drop_to_waorking_set();

                if (amount == 1) { complete_phase(); }
            }

        private:

            struct drop_token {
                    std::uintptr_t next_;
            };

            template <typename T>
            void
            add_to_working_set(T* _ptr)
            {
                std::uintptr_t previous = working_set_.load(std::memory_order_relaxed);

                bool flag = 0;

                if constexpr (std::is_same_v<arrival_token, T>) { flag = kTokenFlag; }

                do
                {

                    _ptr->next_ = previous;

                } while (!working_set_.compare_exchange_weak(
                    previous,
                    reinterpret_cast<std::uintptr_t>(_ptr) | flag,
                    std::memory_order_release,
                    std::memory_order_relaxed));
            }

            void
            add_drop_to_waorking_set()
            {
                drop_token* ptr = new drop_token{};

                std::uintptr_t previous = working_set_.load(std::memory_order_relaxed);

                do
                {

                    ptr->next_ = previous;

                } while (!working_set_.compare_exchange_weak(
                    previous,
                    reinterpret_cast<std::uintptr_t>(ptr) | kDropFlag,
                    std::memory_order_release,
                    std::memory_order_relaxed));
            }

            async_function<>
            complete_phase()
            {
                auto start_window = aquire_window();

                /* Trim window from working set */

                auto current = start_window;
                /* Its either the one in the current  */
                /* root (likely if the user is only   */
                /* using n threads and n == expected_) */
                if (!working_set_.compare_exchange_strong(
                        current,
                        0,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed))
                {

                    /* Or its somewhere down the linked list */
                    iterate_linked_list(
                        current,
                        [start_window](auto* _type)
                        {
                            if (_type->next_ == start_window)
                            {

                                _type->next_ = 0;

                                return false;
                            }
                            else
                            {

                                return true;
                            }
                        });
                }

                /* So now the start_window is a set of waiting  */
                /* frames of size expected_  to be resumed...   */

                /* Compute amount to drop... */
                std::ptrdiff_t amount_to_drop = 0;

                current = start_window;
                iterate_linked_list(
                    current,
                    [&amount_to_drop](auto* _type)
                    {
                        using dec_type = typename std::decay_t<decltype(_type)>;
                        if (std::is_same_v<dec_type, drop_token*>) { ++amount_to_drop; }

                        return true;
                    });

                /* If there actually is a completion function? */
                if constexpr (!std::is_same_v<CompletionFunction, details::no_op>)
                {
                    /* Put us in the correct thread... */
                    if (thread_ != thread_t::any_thread() && engine_->current_id() != thread_)
                    {
                        co_await yield(engine_, thread_);
                    }

                    if constexpr (details::NoThrowAwaitable<CompletionFunction>)
                    {
                        co_await function_;
                    }
                    else
                    {
                        function_();
                    }
                }

                /* Reduce expected */
                expected_ -= amount_to_drop;

                resume_frames(start_window);
            }

            std::uintptr_t
            aquire_window()
            {
                std::ptrdiff_t count = 0;
                std::uintptr_t head  = 0;

                while (!head)
                {
                    auto current = working_set_.load();

                    iterate_linked_list(
                        current,
                        [this, &count, &head, current](auto*)
                        {
                            ++count;

                            if (count >= expected_)
                            {
                                if (!head) { head = current; }
                                else
                                {

                                    head = get_next(head);
                                }
                            }

                            return true;
                        });

                    count = 0;
                }

                return head;
            }

            template <typename Functor>
            void
            iterate_linked_list(std::uintptr_t _ptr, Functor _f)
            {
                while (_ptr)
                {

                    auto flag = _ptr & kFlagMask;

                    if (!flag)
                    {

                        auto token = reinterpret_cast<waiter*>(_ptr);

                        _ptr = token->next_;

                        if (!_f(token)) { return; }
                    }
                    else if (flag == kTokenFlag)
                    {

                        auto token =
                            reinterpret_cast<arrival_token::InternalState*>(_ptr & kAddressMask);

                        _ptr = token->next_;

                        if (!_f(token)) { return; }
                    }
                    else
                    {

                        auto token = reinterpret_cast<drop_token*>(_ptr & kAddressMask);

                        _ptr = token->next_;
                        if (!_f(token)) { return; }
                    }
                }
            }

            std::uintptr_t
            get_next(std::uintptr_t _ptr)
            {
                auto flag = _ptr & kFlagMask;

                if (!flag)
                {

                    auto token = reinterpret_cast<waiter*>(_ptr);

                    return token->next_;
                }
                else if (flag == kTokenFlag)
                {

                    auto token =
                        reinterpret_cast<arrival_token::InternalState*>(_ptr & kAddressMask);

                    return token->next_;
                }
                else
                {

                    auto token = reinterpret_cast<drop_token*>(_ptr & kAddressMask);

                    return token->next_;
                }
            }

            void
            resume_frames(std::uintptr_t _ptr)
            {

                auto count = count_.fetch_add(expected_, std::memory_order_release);

                iterate_linked_list(
                    _ptr,
                    [this](auto* _type)
                    {
                        std::coroutine_handle<> to_resume;
                        thread_t                to_execute_in;
                        using dec_type = typename std::decay_t<decltype(_type)>;
                        if constexpr (std::is_same_v<dec_type, waiter*>)
                        {

                            to_resume     = _type->handle_;
                            to_execute_in = _type->thread_;
                        }
                        else if constexpr (std::is_same_v<
                                               dec_type,
                                               typename arrival_token::InternalState*>)
                        {

                            void* internals = nullptr;
                            if (!_type->handle_.compare_exchange_strong(
                                    internals,
                                    reinterpret_cast<void*>(kFlagMask),
                                    std::memory_order_release,
                                    std::memory_order_relaxed))
                            {

                                /* If it wasnt null, it means there is a coroutine there...*/
                                to_resume     = std::coroutine_handle<>::from_address(internals);
                                to_execute_in = _type->thread_;
                            }
                        }
                        else
                        {

                            /* Clean up drop tokens... */
                            delete _type;
                        }
                        if (to_resume)
                        {
                            engine_->thread_resume(create_generic_event(to_resume), to_execute_in);
                        }

                        return true;
                    });

                /* There are more then working_set_ things waiting */
                if (count <= -expected_)
                {
                    engine_->execute(
                        [this]() noexcept { complete_phase(); },
                        order_t{order::now()},
                        thread_);
                }
            }

            friend class waiter;

            static constexpr std::uintptr_t kDropFlag    = 0b01;
            static constexpr std::uintptr_t kTokenFlag   = 0b10;
            static constexpr std::uintptr_t kFlagMask    = 0b11;
            static constexpr std::uintptr_t kAddressMask = ~kFlagMask;

            engine* engine_;

            std::ptrdiff_t expected_;

            std::atomic<std::uintptr_t> working_set_ = 0;
            std::atomic<std::ptrdiff_t> count_       = 0;

            CompletionFunction function_;
            thread_t           thread_;
    };

}   // namespace zab

#endif /* ZAB_ASYNC_BARRIER_HPP_ */