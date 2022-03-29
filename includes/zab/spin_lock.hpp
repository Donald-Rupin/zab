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
 *  @file spin_lock.cpp
 *
 */

#ifndef ZAB_SPIN_LOCK_HPP_
#define ZAB_SPIN_LOCK_HPP_

#include <atomic>

#include "zab/hardware_interface_size.hpp"

namespace zab {

    /**
     * @brief A atomic based spin lock implementation.
     *
     */
    struct alignas(hardware_constructive_interference_size) spin_lock {

            /**
             * @brief Attempt to acquire the mutex. Does a busy wait until it can acquire the mutex.
             *
             */
            inline void
            lock() noexcept
            {
                while (true)
                {
                    if (!lock_.exchange(true, std::memory_order_acquire)) { return; }

                    while (lock_.load(std::memory_order_relaxed))
                    {
                        __builtin_ia32_pause();
                    }
                }
            }

            /**
             * @brief Attempt to acquire the mutex. Does not wait on failure.
             *
             * @return true If acquired.
             * @return false If not acquired.
             */
            inline bool
            try_lock() noexcept
            {
                return !lock_.load(std::memory_order_relaxed) &&
                       !lock_.exchange(true, std::memory_order_acquire);
            }

            /**
             * @brief Releases the mutex.
             *
             */
            inline void
            unlock() noexcept
            {
                lock_.store(false, std::memory_order_release);
            }

            std::atomic<bool> lock_ alignas(hardware_constructive_interference_size) = {false};
    };

    /**
     * @brief A recursive atomic based spin lock implementation. A thread may lock the mutex more
     * then once.
     *
     */
    struct alignas(hardware_constructive_interference_size) recursive_spin_lock {

            /**
             * @brief Ges the current thread id. This ID is global and unique.
             *
             * @details Creating more then std::numerical_limits<std::size_t>::max() threads that
             *          access a recursive_spin_lock will lead to undefined behaviour.
             *
             * @return size_t The id of the thread.
             */
            static inline size_t
            get_id() noexcept
            {
                static std::atomic<std::size_t> count = 1;
                static thread_local std::size_t id    = count.fetch_add(1);
                return id;
            }

            /**
             * @brief Attempt to acquire the mutex. Does a busy wait until it can acquire the mutex.
             *
             */
            inline void
            lock() noexcept
            {
                auto t_hash = get_id();

                auto curr_thread = thread_.load(std::memory_order_relaxed);

                if (curr_thread == t_hash) { ++count_; }
                else
                {
                    while (true)
                    {
                        size_t empty = 0;
                        if (thread_.compare_exchange_strong(
                                empty,
                                t_hash,
                                std::memory_order_acquire,
                                std::memory_order_relaxed))
                        {
                            ++count_;
                            return;
                        }

                        while (thread_.load(std::memory_order_relaxed) != 0)
                        {
                            __builtin_ia32_pause();
                        }
                    }
                }
            }

            /**
             * @brief Attempt to acquire the mutex. Does not wait on failure.
             *
             * @return true If acquired.
             * @return false If not acquired.
             */
            inline bool
            try_lock() noexcept
            {
                auto t_hash      = get_id();
                auto curr_thread = thread_.load(std::memory_order_relaxed);

                if (!curr_thread)
                {
                    size_t empty = 0;
                    if (thread_.compare_exchange_strong(
                            empty,
                            t_hash,
                            std::memory_order_acquire,
                            std::memory_order_relaxed))
                    {
                        ++count_;
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                }
                else if (curr_thread == t_hash)
                {
                    ++count_;
                    return true;
                }
                else
                {
                    return false;
                }
            }

            /**
             * @brief Attempts to release the mutex. Only is released once `unlock()` is called the
             *       same amount of times `lock()` has been called within the same threads
             * acquirement.
             *
             */
            inline void
            unlock() noexcept
            {
                --count_;
                if (!count_) { thread_.store(0, std::memory_order_release); }
            }

            std::atomic<std::size_t> thread_ alignas(hardware_constructive_interference_size) = {0};
            std::size_t              count_                                                   = {0};
    };

}   // namespace zab

#endif /* ZAB_SPIN_LOCK_HPP_ */
