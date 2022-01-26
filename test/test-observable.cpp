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
 *  @file test-observable.cpp
 *
 */

#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/observable.hpp"
#include "zab/reusable_future.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    static constexpr auto kNumberThreads = 6u;

    int
    test_observer_thread();

    int
    run_test()
    {
        return test_observer_thread();
    }

    class test_observer_thread_class : public engine_enabled<test_observer_thread_class> {
        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr auto kRounds = 1000;

            test_observer_thread_class() = default;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                failed_ = (
                    // co_await test_push_pop(0) || co_await test_push_pop(1) ||
                    // co_await test_push_pop(10) || co_await test_push_pop(100) ||
                    // co_await test_resume(0) || co_await test_resume(1) ||
                    // co_await test_resume(10) || co_await test_resume(100) ||
                    co_await test_multi_thread());

                engine_->stop();
            }

            simple_future<bool>
            test_push_pop(std::size_t _size)
            {
                observable<int, int> ob(engine_);

                auto connection_1 = co_await ob.connect();
                auto connection_2 = co_await ob.connect();

                for (int i = 0; i < kRounds; ++i)
                {
                    for (std::size_t j = 0; j < _size; ++j)
                    {
                        ob.async_emit(i, i + 1);
                    }

                    for (std::size_t j = 0; j < _size; ++j)
                    {
                        /* These should not block as there are events to get */
                        auto con_g_1 = co_await connection_1;
                        auto con_g_2 = co_await connection_2;

                        const auto& [c11, c12] = con_g_1.event();
                        const auto& [c21, c22] = con_g_2.event();

                        /* must be same values... */
                        if (expected(c11, i) || expected(c12, i + 1) || expected(c21, i) ||
                            expected(c22, i + 1))
                        {
                            co_return true;
                        }

                        /* Will also be the same object*/

                        if (expected(&c11, &c21) || expected(&c12, &c22)) { co_return true; }
                    }
                }

                co_return false;
            }

            using string_observable = observable<std::string, std::string, std::string>;

            simple_future<bool>
            test_resume(std::size_t _size)
            {
                finished = 0;

                string_observable ob(engine_);

                for (std::size_t j = 0; j < _size; ++j)
                {
                    test_resume(ob);
                }

                co_await yield(order::in_seconds(1));

                /* should not resume until all are finished */
                co_await ob.emit("1", "2", "3");

                if (expected(finished, _size)) { co_return true; }

                /* should not block as all observers should of de registered */
                co_await ob.emit("1", "2", "3");

                co_return false;
            }

            async_function<>
            test_resume(string_observable& _ob)
            {
                auto connection = co_await _ob.connect();

                auto e = co_await connection;

                const auto& [_1, _2, _3] = e.event();

                if (expected(_1, "1") || expected(_2, "2") || expected(_3, "3")) { finished = 0; }
                else
                {
                    ++finished;
                }
            }

            simple_future<bool>
            test_multi_thread()
            {
                t_finished.store(0);

                string_observable ob(engine_);

                for (std::uint16_t i = 0; i < kNumberThreads; ++i)
                {
                    test_resume(ob, thread_t{i});
                }

                co_await yield(order::in_seconds(1));

                for (int i = 0; i < kRounds; ++i)
                {
                    ob.async_emit(std::to_string(i), std::to_string(i + 1), std::to_string(i + 2));
                }

                co_await ob.await_disconnect();

                if (expected(t_finished.load(), kNumberThreads * kRounds)) { co_return true; }

                co_return false;
            }

            async_function<>
            test_resume(string_observable& _ob, thread_t _thread)
            {
                co_await yield(_thread);

                auto connection = co_await _ob.connect();

                for (int i = 0; i < kRounds; ++i)
                {
                    auto e = co_await connection;

                    const auto& [_1, _2, _3] = e.event();
                    if (expected(_1, std::to_string(i)) || expected(_2, std::to_string(i + 1)) ||
                        expected(_3, std::to_string(i + 2)))
                    {
                        t_finished.store(0);

                        abort();
                        engine_->stop();
                    }
                    else
                    {
                        t_finished++;
                    }
                }
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;

            std::size_t finished = 0;

            std::atomic<std::size_t> t_finished;
    };

    int
    test_observer_thread()
    {
        engine engine(engine::configs{kNumberThreads});

        test_observer_thread_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
        ;
    }

}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}
