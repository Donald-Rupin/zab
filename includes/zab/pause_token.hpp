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
 *  @file pause_token.hpp
 *
 */

#ifndef ZAB_PAUSE_TOKEN_HPP_
#define ZAB_PAUSE_TOKEN_HPP_

#include <atomic>

#include "zab/engine.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class pause_token {

            struct pauser {

                    bool
                    await_suspend(std::coroutine_handle<> _awaiter) noexcept
                    {
                        handle_ = _awaiter;
                        return token_.pause(this);
                    }

                    bool
                    await_ready() const noexcept
                    {
                        return !token_.paused();
                    }

                    void
                    await_resume() const noexcept
                    { }

                    pause_token&            token_;
                    thread_t                thread_       = thread_t{};
                    pauser*                 next_waiting_ = nullptr;
                    std::coroutine_handle<> handle_       = nullptr;
            };

        public:

            pause_token(engine* _engine) : engine_(_engine) { }

            [[nodiscard]] bool
            paused() const noexcept
            {
                return !(resuming_.load(std::memory_order_acquire) & kUnpausedFlag);
            }

            void
            pause() noexcept
            {
                auto current_state = resuming_.load(std::memory_order_acquire);
                if (current_state != kUnpausedFlag) { return; }
                while (!resuming_.compare_exchange_weak(
                           current_state,
                           0,
                           std::memory_order_acquire,
                           std::memory_order_relaxed) &&
                       current_state == kUnpausedFlag)
                { };
            }

            void
            unpause() noexcept
            {
                auto old_value = resuming_.exchange(kUnpausedFlag, std::memory_order_release);

                if (old_value == kUnpausedFlag) { return; }

                auto old_pause = reinterpret_cast<pauser*>(old_value);

                while (old_pause)
                {
                    /* Save it here because if the yield is  */
                    /* In a different thread, the memory may */
                    /* get deleted before we dereference...  */
                    auto tmp = old_pause->next_waiting_;

                    engine_->resume(old_pause->handle_, order_t{order::now()}, old_pause->thread_);

                    old_pause = tmp;
                }
            }

            pauser operator co_await() noexcept
            {
                return pauser{*this, engine_->get_event_loop().current_id()};
            }

            bool
            pause(pauser* pauser_) noexcept
            {
                /* We are ABA free here (even under re-use)      */
                /* This is because with the two operations to    */
                /* the coroutine handle linked list are push     */
                /* and clear all.                                */
                /* In any case, we do not care that the list was */
                /* modified, only that our next waiting is still */
                /* the last thing on the list. */
                std::uintptr_t previous = resuming_.load(std::memory_order_acquire);
                do
                {

                    if (previous == kUnpausedFlag) { return false; }

                    pauser_->next_waiting_ = reinterpret_cast<pauser*>(previous);
                    if (resuming_.compare_exchange_weak(
                            previous,
                            reinterpret_cast<std::uintptr_t>(pauser_),
                            std::memory_order_acquire,
                            std::memory_order_relaxed))
                    {

                        return true;
                    }

                } while (true);

                return true;
            }

        private:

            static constexpr std::uintptr_t kUnpausedFlag = 0b1;

            friend struct pauser;

            engine* engine_;

            std::atomic<std::uintptr_t> resuming_ = 0;
    };

}   // namespace zab

#endif /* ZAB_PAUSE_TOKEN_HPP_ */