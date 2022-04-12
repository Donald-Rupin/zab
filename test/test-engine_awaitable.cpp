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
 *  @file test-engine_awaitble.cpp
 *
 */

#include <optional>
#include <thread>

#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/reusable_promise.hpp"
#include "zab/simple_promise.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_async_function();

    int
    test_promise_function();

    int
    test_recursive_promise_function();

    int
    test_pause_function();

    int
    test_reusable_promise_function();

    int
    test_proxy();

    int
    run_test()
    {
        return test_async_function() || test_promise_function() ||
               test_recursive_promise_function() || test_pause_function() ||
               test_reusable_promise_function() || test_proxy();
    }

    class test_async_class : public engine_enabled<test_async_class> {

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
                co_await yield(now(), thread_t{kInitialiseThread + 1});

                auto thread_number = engine_->current_id();

                if (expected(thread_number.thread_, kInitialiseThread + 1)) { engine_->stop(); }

                co_await yield(now(), thread_t{kInitialiseThread});

                thread_number = engine_->current_id();

                if (expected(thread_number.thread_, kInitialiseThread)) { engine_->stop(); }

                failed_ = false;

                engine_->stop();
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
    test_async_function()
    {
        engine engine(engine::configs{2, engine::configs::kExact});

        test_async_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_promise_class : public engine_enabled<test_promise_class> {

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
                /* This should not pause... Not sure how to test*/
                co_await constant_void();

                co_await void_promise();

                /* This should not pause... Not sure how to test*/
                std::optional<size_t> promise = co_await constant_typed_promise();

                if (expected((bool) promise, true) || expected(*promise, 2u))
                {
                    engine_->stop();
                    co_return;
                }

                promise = co_await typed_promise();

                if (promise && *promise == 1) { failed_ = false; }

                engine_->stop();
            }

            simple_future<>
            constant_void() noexcept
            {
                co_return;
            }

            simple_future<>
            void_promise() noexcept
            {
                co_await yield(now(), thread(kInitialiseThread));

                auto thread_number = engine_->current_id();

                if (expected(thread_number.thread_, kInitialiseThread)) { engine_->stop(); }
            }

            simple_future<size_t>
            constant_typed_promise() noexcept
            {
                co_return 2;
            }

            simple_future<size_t>
            typed_promise() noexcept
            {
                co_await yield(now(), thread_t{kInitialiseThread});

                auto thread_number = engine_->current_id();

                if (expected(thread_number.thread_, kInitialiseThread))
                {
                    engine_->stop();
                    co_return std::nullopt;
                }

                co_await yield(now(), thread_t{kInitialiseThread});

                thread_number = engine_->current_id();

                if (expected(thread_number.thread_, kInitialiseThread))
                {
                    engine_->stop();
                    co_return std::nullopt;
                }

                co_return 1;
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
    test_promise_function()
    {
        engine engine(engine::configs{2, engine::configs::kExact});

        test_promise_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_recursive_promise_class : public engine_enabled<test_recursive_promise_class> {

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
                /* This should not pause... Not sure how to test*/
                std::optional<size_t> promise = co_await constant_recursive_promise(0, 100);

                if (expected((bool) promise, true) || expected(*promise, 1u))
                {
                    engine_->stop();
                    co_return;
                }

                promise = co_await recursive_promise(0, 100);

                if (promise && *promise == 1) { failed_ = false; }

                engine_->stop();
            }

            simple_future<size_t>
            constant_recursive_promise(size_t _start, size_t _max) noexcept
            {
                if (_start == _max) { co_return 1; }
                else
                {

                    co_return co_await constant_recursive_promise(_start + 1, _max);
                }
            }

            simple_future<size_t>
            recursive_promise(size_t _start, size_t _max) noexcept
            {
                if (_start == _max) { co_return 1; }
                else
                {

                    /* yield to force the frame to be saved... */
                    co_await yield(now(), thread_t{kInitialiseThread});

                    auto thread_number = engine_->current_id();

                    if (expected(thread_number.thread_, kInitialiseThread)) { engine_->stop(); }

                    co_return co_await recursive_promise(_start + 1, _max);
                }
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
    test_recursive_promise_function()
    {
        engine engine(engine::configs{2, engine::configs::kExact});

        test_recursive_promise_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_pause_class : public engine_enabled<test_pause_class> {

        public:

            static constexpr auto kInitialiseThread = 0;
            static constexpr auto kDefaultThread    = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                pause_pack* pack;

                /* Explicitly yield some code for later */
                /* Should execute in the kDefaultThread */
                code_block(
                    [this, &pack]() noexcept
                    {
                        if (expected(engine_->current_id().thread_, kDefaultThread))
                        {
                            engine_->stop();
                            return;
                        }

                        pack->data_ = 1;
                        unpause(*pack);
                    });

                auto p = co_await pause([&pack](auto* _p) noexcept { pack = _p; });

                if (expected(p.data_, 1u)) { failed_ = true; }
                else
                {

                    failed_ = false;
                }

                engine_->stop();
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
    test_pause_function()
    {
        engine engine(engine::configs{2, engine::configs::kExact});

        test_pause_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_reusable_promise_class : public engine_enabled<test_reusable_promise_class> {

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
                auto function = constant_typed_promise();

                std::optional result = co_await function;

                if (expected((bool) result, true) || expected(*result, 42u))
                {

                    engine_->stop();
                    co_return;
                }

                /* Can only get 1 value from it! */
                result = co_await function;
                if (expected(false, (bool) result))
                {

                    engine_->stop();
                    co_return;
                }

                for (size_t i = 0; i < 100; ++i)
                {

                    auto function = typed_promise(i);

                    for (size_t j = 0; j < i; ++j)
                    {

                        result = co_await function;

                        if (expected((bool) result, true) || expected(*result, j))
                        {

                            engine_->stop();
                            co_return;
                        }
                    }

                    result = co_await function;
                    /* If we do any more - it will just return nullopt. */
                    if (expected((bool) result, false))
                    {

                        engine_->stop();
                        co_return;
                    }
                }

                auto infinite_function = infinite_typed_promise();
                for (size_t i = 0; i < 1000; ++i)
                {

                    result = co_await infinite_function;
                    if (expected((bool) result, true) || expected(*result, i))
                    {

                        engine_->stop();
                        co_return;
                    }
                }

                failed_ = false;
                engine_->stop();
            }

            reusable_future<size_t>
            constant_typed_promise() noexcept
            {
                co_return 42;
            }

            reusable_future<size_t>
            typed_promise(size_t _amount) noexcept
            {
                for (size_t i = 0; i < _amount; i++)
                {

                    co_await yield();

                    co_yield i;
                }

                co_return std::nullopt;
            }

            reusable_future<size_t>
            infinite_typed_promise() noexcept
            {
                size_t loops = 0;
                while (true)
                {

                    co_await yield();

                    co_yield loops;

                    ++loops;
                }

                co_return std::nullopt;
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
    test_reusable_promise_function()
    {
        engine engine(engine::configs{2, engine::configs::kExact});

        test_reusable_promise_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_proxy_class : public engine_enabled<test_proxy_class> {

        public:

            static constexpr auto kInitialiseThread = 0;

            bool
            failed()
            {
                return failed_;
            }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                auto thread_number = engine_->current_id();

                if (expected(thread_number.thread_, 0)) { engine_->stop(); }

                test_function_1_proxy(1, 2, 3, 4);

                thread_number = engine_->current_id();

                if (expected(thread_number.thread_, 0)) { engine_->stop(); }

                auto result = co_await test_function_2_proxy(3);

                thread_number = engine_->current_id();

                if (expected(thread_number.thread_, 0)) { engine_->stop(); }

                if (expected(result, 3)) { engine_->stop(); }

                auto generator = test_function_3_proxy(10, 20);

                size_t count = 10;
                while (!generator.complete())
                {

                    auto result = co_await generator;

                    thread_number = engine_->current_id();

                    if (expected(thread_number.thread_, 0)) { engine_->stop(); }

                    if (expected((bool) result, true) || expected(*result, count))
                    {
                        engine_->stop();
                    }

                    ++count;
                }

                if (expected(21u, count))
                {
                    engine_->stop();
                    co_return;
                }

                engine_->stop();
                failed_ = false;
                co_return;
            }

            async_function<>
            test_function_1_proxy(int, int, int, int)
            {
                return proxy(&test_proxy_class::test_function_1, thread_t{1}, 1, 2, 3, 4);
            }

            async_function<>
            test_function_1(int, int, int, int) const
            {

                auto thread_number = engine_->current_id();

                if (expected(thread_number.thread_, 1)) { engine_->stop(); }

                co_await yield(now(), thread(1));
            }

            guaranteed_future<int>
            test_function_2(int _argument) const
            {
                auto thread_number = engine_->current_id();

                if (expected(thread_number.thread_, 2)) { engine_->stop(); }

                co_await yield(now(), thread(2));

                co_return _argument;
            }

            guaranteed_future<int>
            test_function_2_proxy(int _argument_test)
            {
                return proxy(&test_proxy_class::test_function_2, thread_t{2}, _argument_test);
            }

            reusable_future<size_t>
            test_function_3_proxy(size_t _start, size_t _stop)
            {
                return proxy(&test_proxy_class::test_function_3, thread_t{3}, _start, _stop);
            }

        private:

            reusable_future<size_t>
            test_function_3(size_t _start, size_t _stop)
            {
                while (_start < _stop)
                {

                    auto thread_number = engine_->current_id();

                    if (expected(thread_number.thread_, 3)) { engine_->stop(); }

                    co_yield _start;

                    thread_number = engine_->current_id();

                    if (expected(thread_number.thread_, 3)) { engine_->stop(); }

                    ++_start;

                    co_await yield(now(), thread(3));
                }

                co_return _stop;
            }

            bool failed_ = true;
    };

    int
    test_proxy()
    {
        engine engine(engine::configs{4});

        test_proxy_class test;

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