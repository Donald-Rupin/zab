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
 *  MIT License
 *
 *  Copyright (c) 2021 Donald-Rupin
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 *  @file test-defer_block_promise.cpp
 *
 */

#include "zab/async_latch.hpp"
#include "zab/defer_block_promise.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_defer_class();

    int
    run_test()
    {
        return test_defer_class();
    }

    class defer_class : public engine_enabled<defer_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                async_latch latch(get_engine(), 6);

                no_suspension(latch);
                suspension(latch);
                recursive(latch);
                purge(latch);
                local_var_access(latch);

                co_await latch.arrive_and_wait();

                get_engine()->stop();
            }

            async_function<defer_block_promise>
            no_suspension(async_latch& _latch) noexcept
            {

                co_yield defer_block([this]() noexcept { ++count_; });

                _latch.count_down();
            }

            async_function<defer_block_promise>
            suspension(async_latch& _latch) noexcept
            {
                co_await yield();

                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();

                _latch.count_down();
            }

            async_function<defer_block_promise>
            recursive(async_latch& _latch) noexcept
            {
                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();

                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();

                _latch.count_down();
            }

            async_function<defer_block_promise>
            purge(async_latch& _latch) noexcept
            {
                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();

                co_yield defer_block([this]() noexcept { ++count_; });

                co_yield purge_block{};

                _latch.count_down();
            }

            async_function<defer_block_promise>
            local_var_access(async_latch& _latch)
            {
                int x = 0;

                co_yield defer_block(
                    [this, &x]() mutable noexcept
                    {
                        ++count_;

                        /* Executed in reverse order... */
                        if (expected(1, x))
                        {
                            get_engine()->stop();
                            count_ += 100;
                        }
                    });

                co_await yield();

                co_yield defer_block(
                    [this, &x]() noexcept
                    {
                        ++count_;

                        if (expected(0, x))
                        {
                            get_engine()->stop();
                            count_ += 100;
                        }

                        ++x;
                    });

                _latch.count_down();
            }

            bool
            failed()
            {
                return count_ != 6;
            }

        private:

            size_t count_ = 0;
    };

    int
    test_defer_class()
    {
        engine engine(event_loop::configs{});

        defer_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}