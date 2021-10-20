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
 *  @file test-first_of.cpp
 *
 */

#include <chrono>

#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/first_of.hpp"
#include "zab/reusable_promise.hpp"
#include "zab/simple_future.hpp"
#include "zab/simple_promise.hpp"
#include "zab/strong_types.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_single_thread();

    int
    test_mulit_thread();

    int
    run_test()
    {
        return test_single_thread() || test_mulit_thread();
    }

    class test_single_thread_class : public engine_enabled<test_single_thread_class> {

        public:

            static constexpr auto kInitialiseThread = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                auto begin = std::chrono::steady_clock::now();

                std::variant<float, int, promise_void> result =
                    co_await first_of(engine_, timer<float>(2, 1.02), timer<int>(1, 42), timer(3));

                auto end = std::chrono::steady_clock::now();

                std::size_t duration =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();

                if (duration >= order::in_seconds(2))
                {
                    std::cout << "Timer failed."
                              << "\n";
                    engine_->stop();
                }

                /* lets let the others finish... and they should not interfere with the internal
                 * state */
                co_await yield(order::in_seconds(4));

                /* int should win and be of value 42 */
                auto* value = std::get_if<int>(&result);

                if (expected((bool) value, true))
                {
                    engine_->stop();
                    co_return;
                }

                if (expected(*value, 42))
                {
                    engine_->stop();
                    co_return;
                }

                failed_ = false;

                engine_->stop();

                co_return;
            }

            template <typename T>
            guaranteed_future<T>
            timer(std::size_t _time, T _value)
            {
                co_await yield(order::in_seconds(_time));

                co_return _value;
            }

            simple_future<>
            timer(std::size_t _time)
            {
                co_await yield(order::in_seconds(_time));

                co_return;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;
    };

    int
    test_single_thread()
    {
        engine engine(event_loop::configs{1, event_loop::configs::kExact});

        test_single_thread_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_mulit_thread_class : public engine_enabled<test_mulit_thread_class> {

        public:

            static constexpr auto kInitialiseThread = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                auto begin = std::chrono::steady_clock::now();

                std::variant<float, int, promise_void> result = co_await first_of(
                    engine_,
                    timer<float>(2, 1.02, thread_t{1}),
                    timer<int>(1, 42, thread_t{1}),
                    timer(3, thread_t{1}));

                auto end = std::chrono::steady_clock::now();

                std::size_t duration =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();

                if (duration >= order::in_seconds(2))
                {
                    std::cout << "Timer failed."
                              << "\n";
                    engine_->stop();
                }

                /* lets let the others finish... and they should not interfere with the internal
                 * state */
                co_await yield(order::in_seconds(4));

                /* int should win and be of value 42 */
                auto* value = std::get_if<int>(&result);

                if (expected((bool) value, true))
                {
                    engine_->stop();
                    co_return;
                }

                if (expected(*value, 42))
                {
                    engine_->stop();
                    co_return;
                }

                failed_ = false;

                engine_->stop();

                co_return;
            }

            template <typename T>
            guaranteed_future<T>
            timer(std::size_t _time, T _value, thread_t _thread)
            {
                co_await yield(_thread);

                co_await yield(order::in_seconds(_time));

                co_return _value;
            }

            simple_future<>
            timer(std::size_t _time, thread_t _thread)
            {
                co_await yield(_thread);

                co_await yield(order::in_seconds(_time));

                co_return;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;
    };

    int
    test_mulit_thread()
    {
        engine engine(event_loop::configs{4, event_loop::configs::kExact});

        test_mulit_thread_class test;

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