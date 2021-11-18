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
 *  @file async_latch.hpp
 *
 */

#ifndef ZAB_ASYNC_LATCH_HPP_
#define ZAB_ASYNC_LATCH_HPP_

#include <atomic>
#include <deque>

#include "zab/pause_token.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class async_latch {

        public:

            async_latch(engine* _engine, std::ptrdiff_t _amount)
                : count_(_amount), complete_(_engine)
            { }

            ~async_latch() = default;

            void
            count_down(std::ptrdiff_t _amount = 1) noexcept
            {
                if (auto number = count_.fetch_sub(_amount, std::memory_order_acq_rel);
                    number > 0 && number <= _amount)
                {

                    complete_.unpause();
                }
            }

            [[nodiscard]] bool
            try_wait() noexcept
            {
                return count_.load() > 0;
            }

            [[nodiscard]] auto
            wait() noexcept
            {
                return complete_.operator co_await();
            }

            [[nodiscard]] auto
            arrive_and_wait(std::ptrdiff_t _amount = 1) noexcept
            {
                count_down(_amount);

                return complete_.operator co_await();
            }

        private:

            void
            notify();

            std::atomic<std::ptrdiff_t> count_;

            pause_token complete_;
    };

}   // namespace zab

#endif /* ZAB_ASYNC_LATCH_HPP_ */