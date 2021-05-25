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
 *  @file test-async_mutex.cpp
 *
 */

#include <cstddef>
#include <memory>
#include <optional>
#include <thread>

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_not_paused();

    int
    test_multi_thread_mutex();

    int
    run_test()
    {
        return test_not_paused() || test_multi_thread_mutex();
    }

    class test_not_paused_class : public engine_enabled<test_not_paused_class> {

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
                mutex_ = std::make_shared<async_mutex>(get_engine());

                count_ = 0;

                no_defer();

                count_ = 1;

                /* This will lock until `lock()` has completed */
                {
                    auto lock = co_await *mutex_;
                }

                co_await in_order();

                {
                    auto lock = co_await *mutex_;
                }

                if (expected(count_, 4u)) { get_engine()->stop(); }
                else
                {

                    failed_ = false;
                }

                get_engine()->stop();
            }

            async_function<>
            no_defer() noexcept
            {
                {
                    auto lck = co_await *mutex_;
                }

                if (expected(count_, 0u)) { get_engine()->stop(); }

                {
                    auto lck = co_await *mutex_;
                }

                if (expected(count_, 0u)) { get_engine()->stop(); }

                {
                    auto lck = co_await *mutex_;

                    /* This will be woken up by the deconstruction */
                    /* But the yield will execute after we finish... */
                    lock();
                }

                if (expected(count_, 0u)) { get_engine()->stop(); }
            }

            async_function<>
            lock() noexcept
            {
                auto lock = co_await *mutex_;

                if (expected(count_, 1u)) { get_engine()->stop(); }
            }

            simple_future<>
            in_order() noexcept
            {
                {
                    auto lock = co_await *mutex_;

                    /* (1) */
                    add_one(1);
                }

                /* (1) Wont have been woken up yet... (cause there is only 1 thread)*/
                if (expected(count_, 1u)) { get_engine()->stop(); }

                {
                    /* But now we wait on (1) */
                    auto lock = co_await *mutex_;

                    /* (2) */
                    add_one(2);
                }

                /* (2) Wont have been woken up yet... (cause there is only 1 thread)*/
                if (expected(count_, 2u)) { get_engine()->stop(); }

                {
                    /* But now we wait on (2) */
                    auto lock = co_await *mutex_;

                    /* (3) */
                    add_one(3);
                }

                /* (3) Wont have been woken up yet... (cause there is only 1 thread)*/
                if (expected(count_, 3u)) { get_engine()->stop(); }
            }

            async_function<>
            add_one(size_t _expected) noexcept
            {
                auto lock = co_await *mutex_;
                if (expected(count_, _expected)) { get_engine()->stop(); }

                ++count_;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            std::shared_ptr<async_mutex> mutex_;

            size_t count_ = 0;

            bool failed_ = true;
    };

    int
    test_not_paused()
    {
        engine engine(event_loop::configs{2});

        test_not_paused_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class multi_thread_mutex_class : public engine_enabled<multi_thread_mutex_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr std::uint16_t kNumberThreads = 10;
            static constexpr size_t   kNumberOpps    = 1000;

            void
            initialise() noexcept
            {
                mutex_ = std::make_shared<async_mutex>(get_engine());
                run();
            }

            async_function<>
            run() noexcept
            {
                co_await yield();

                for (std::uint16_t i = 0; i < kNumberThreads; ++i)
                {
                    do_thread_lock(thread_t{i});
                }
            }

            async_function<>
            do_thread_lock(thread_t _thread) noexcept
            {
                /* Go into our thread */
                co_await yield(now(), _thread);

                for (size_t i = 0; i < kNumberOpps; ++i)
                {

                    /* Acquire lock */
                    auto lock = co_await *mutex_;

                    /* Acquire resource */
                    current_thread_ = _thread;

                    for (size_t j = 0; j < (kNumberOpps / 2) + 2; ++j)
                    {

                        /* Do some async stuff*/
                        co_await yield(now(), _thread);

                        /* Is still ours! */
                        if (expected(current_thread_, _thread)) { get_engine()->stop(); }
                    }

                    /* release lock */
                }

                if (count_.fetch_add(1) == kNumberThreads - 1)
                {
                    get_engine()->stop();
                    failed_ = false;
                }
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            std::shared_ptr<async_mutex> mutex_;

            thread_t current_thread_ = thread_t{0};

            std::atomic<std::uint16_t> count_ = 0;

            bool failed_ = true;
    };

    int
    test_multi_thread_mutex()
    {
        engine engine(event_loop::configs{multi_thread_mutex_class::kNumberThreads});

        multi_thread_mutex_class test;

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