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

    struct alignas(hardware_constructive_interference_size) spin_lock {

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

            inline bool
            try_lock() noexcept
            {
                return !lock_.load(std::memory_order_relaxed) &&
                       !lock_.exchange(true, std::memory_order_acquire);
            }

            inline void
            unlock() noexcept
            {
                lock_.store(false, std::memory_order_release);
            }

            std::atomic<bool> lock_ alignas(hardware_constructive_interference_size) = {false};
    };

}   // namespace zab

#endif /* ZAB_SPIN_LOCK_HPP_ */
