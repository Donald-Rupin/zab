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
 *  @file async_mutex.hpp
 *
 */

#ifndef ZAB_async_mutex_HPP_
#define ZAB_async_mutex_HPP_

#include <atomic>
#include <coroutine>
#include <cstddef>

#include "zab/async_semaphore.hpp"

namespace zab {

    class async_mutex {

        public:

            struct async_lock_guard {

                    ~async_lock_guard()
                    {
                        if (sem_) { sem_->release(); }
                    }

                    async_lock_guard(async_binary_semaphore* _sem) : sem_(_sem) { }

                    async_lock_guard(const async_lock_guard& _copy) = delete;

                    async_lock_guard(async_lock_guard&& _move) : sem_(_move.sem_)
                    {
                        _move.sem_ = nullptr;
                    }

                    async_binary_semaphore* sem_;
            };

            async_mutex(engine* _engine) : sem_(_engine, true) { }

            async_mutex(const async_mutex& _engine) = delete;

            async_mutex(async_mutex& _engine) = delete;

            ~async_mutex() = default;

            [[nodiscard]] inline bool
            try_lock() noexcept
            {
                return sem_.try_aquire();
            }

            void
            unlock() noexcept
            {
                sem_.release();
            }

            /**
             * @brief      lock()
             *
             * @return     Locks the mutex.
             *
             */
            auto operator co_await() noexcept
            {
                struct : public async_binary_semaphore::waiter {
                        [[nodiscard]] async_lock_guard
                        await_resume() const noexcept
                        {
                            return async_lock_guard{&semaphore_};
                        }

                } waiter{{sem_}};

                return waiter;
            }

        private:

            async_binary_semaphore sem_;
    };

}   // namespace zab

#endif /* ZAB_async_mutex_HPP_ */