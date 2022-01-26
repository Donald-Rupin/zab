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
 *  @file test-async_latch.cpp
 *
 */

#include <cstddef>
#include <memory>
#include <optional>
#include <thread>

#include "zab/async_function.hpp"
#include "zab/async_latch.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_single_thread();
    int
    test_multi_thread();

    int
    run_test()
    {
        return test_single_thread() || test_multi_thread();
    }

    class test_single_thread_class : public engine_enabled<test_single_thread_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            test_single_thread_class(size_t _threads) : threads_(_threads) { }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                failed_ = !co_await do_test();

                ++test_run_;

                if (test_run_ == test_count_) { engine_->stop(); }
            }

            simple_future<bool>
            do_test() noexcept
            {
                latch_ = std::make_shared<async_latch>(engine_, threads_);

                for (size_t go = 0; go < threads_; ++go)
                {
                    if (expected(count_, 0u)) { co_return false; }

                    aquire();

                    co_await yield();
                }

                co_await yield();

                if (expected(count_, threads_)) { co_return false; }

                co_return true;
            }

            async_function<>
            aquire() noexcept
            {
                co_await latch_->arrive_and_wait();

                ++count_;
            }

            bool
            failed()
            {
                return failed_;
            }

            static size_t test_count_;

            static size_t test_run_;

        private:

            size_t threads_;

            size_t count_ = 0;

            std::shared_ptr<async_latch> latch_;

            bool failed_ = true;
    };

    size_t test_single_thread_class::test_count_ = 0;
    size_t test_single_thread_class::test_run_   = 0;

    int
    test_single_thread()
    {
        engine engine(engine::configs{1});

        test_single_thread_class test2(2);
        test_single_thread_class test5(5);
        test_single_thread_class test9(9);
        test_single_thread_class test16(16);
        test_single_thread_class test21(21);

        test_single_thread_class::test_count_ = 5;

        test2.register_engine(engine);
        test5.register_engine(engine);
        test9.register_engine(engine);
        test16.register_engine(engine);
        test21.register_engine(engine);

        engine.start();

        return test2.failed() || test5.failed() || test9.failed() || test16.failed() ||
               test21.failed();
    }

    class test_multi_thread_class : public engine_enabled<test_multi_thread_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            test_multi_thread_class(size_t _threads) : threads_(_threads), count_(0) { }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                failed_ = !co_await do_test();

                engine_->stop();
            }

            simple_future<bool>
            do_test() noexcept
            {
                latch_ = std::make_shared<async_latch>(engine_, threads_ + 1);

                for (std::uint16_t go = 0; go < threads_; ++go)
                {

                    if (expected(count_.load(), 0u)) { co_return false; }

                    aquire(thread_t{go});

                    co_await yield();
                }

                co_await latch_->arrive_and_wait();

                co_await yield(order_t{order::now() + order::seconds(2)});

                if (expected(count_.load(), threads_)) { co_return false; }

                co_return true;
            }

            async_function<>
            aquire(thread_t _thread) noexcept
            {
                co_await yield(now(), _thread);

                co_await latch_->arrive_and_wait();

                if (expected(engine_->current_id(), _thread)) { co_return; }

                ++count_;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            size_t threads_;

            std::atomic<size_t> count_;

            std::shared_ptr<async_latch> latch_;

            bool failed_ = true;
    };

    int
    test_multi_thread()
    {
        auto lam = [](std::uint16_t _thread)
        {
            engine engine(engine::configs{
                .threads_         = (std::uint16_t)(_thread + 1),
                .opt_             = engine::configs::kAtLeast,
                .affinity_set_    = false,
                .affinity_offset_ = 0});

            test_multi_thread_class test(_thread);
            test.register_engine(engine);
            engine.start();

            return test.failed();
        };

        return lam(6) || lam(13) || lam(18) || lam(24);
    }
}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}