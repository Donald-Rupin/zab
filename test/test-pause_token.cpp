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
 *  @file test-pause_token.cpp
 *
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/pause_token.hpp"
#include "zab/simple_promise.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_basic();

    int
    test_multi_thread_pause();

    int
    run_test()
    {
        return test_basic() || test_multi_thread_pause();
    }

    class test_basic_class : public engine_enabled<test_basic_class> {

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
                failed_ =
                    !(co_await test_done() || co_await test_one() || co_await test_many(11) ||
                      co_await test_many(42) || co_await test_many(97) || co_await test_many(150));

                get_engine()->stop();
            }

            simple_future<bool>
            test_done() noexcept
            {
                pause_token pt(get_engine());

                if (expected(pt.paused(), true)) { co_return false; }

                pt.unpause();

                if (expected(pt.paused(), false)) { co_return false; }

                /* Should result strait away */
                co_await pt;

                co_return true;
            }

            simple_future<bool>
            test_one() noexcept
            {
                pause_token pt(get_engine());

                if (expected(pt.paused(), true)) { co_return false; }

                /* Since this is in the same thread it wont happen until yield */
                unpause(pt);

                if (expected(pt.paused(), true)) { co_return false; }

                /* Should pause*/
                co_await pt;

                co_return true;
            }

            async_function<>
            unpause(pause_token& _token) noexcept
            {
                co_await yield();

                _token.unpause();
            }

            simple_future<bool>
            test_many(size_t _amount) noexcept
            {
                pause_token pt(get_engine());

                if (expected(pt.paused(), true)) { co_return false; }

                size_t count = 0;
                for (size_t i = 0; i < _amount; ++i)
                {
                    pause(pt, count);
                }

                /* Will yield all the others! */
                pt.unpause();

                /* now the paused should of beet us. */
                co_await yield();

                if (expected(count, _amount)) { co_return false; }

                co_return true;
            }

            async_function<>
            pause(pause_token& _token, size_t& count) noexcept
            {
                co_await _token;

                ++count;
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
    test_basic()
    {
        engine engine(event_loop::configs{1});

        test_basic_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_multi_thread_pause_class : public engine_enabled<test_multi_thread_pause_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr std::uint16_t kNumberThreads = 10;
            static constexpr size_t        kNumberOpps    = 50000;

            void
            initialise() noexcept
            {
                pause_ = std::make_shared<pause_token>(get_engine());
                run();
            }

            async_function<>
            run() noexcept
            {
                co_await yield();

                for (std::uint16_t i = 0; i < kNumberThreads; ++i)
                {
                    do_thread_pause(thread_t{i});
                }
            }

            async_function<>
            do_thread_pause(thread_t _thread) noexcept
            {
                /* Go into our thread */
                co_await yield(now(), _thread);

                for (size_t i = 0; i < kNumberOpps; ++i)
                {

                    /* pause */
                    pause();

                    if ((size_t) (kNumberOpps % ((_thread.thread_ + 1 * 5))) ==
                        (size_t) (_thread.thread_ - 1))
                    {
                        if (pause_->paused()) { pause_->unpause(); }
                        else
                        {

                            pause_->pause();
                        }
                    }
                }

                if (pause_->paused()) { pause_->unpause(); }
            }

            async_function<>
            pause() noexcept
            {
                co_await *pause_;

                count_++;

                if (count_.load() == kNumberOpps * kNumberThreads) { get_engine()->stop(); }
            }

            bool
            failed()
            {
                return count_.load() != kNumberOpps * kNumberThreads;
            }

        private:

            std::shared_ptr<pause_token> pause_;

            std::atomic<size_t> count_ = 0;

            bool failed_ = true;
    };

    int
    test_multi_thread_pause()
    {
        engine engine(event_loop::configs{test_multi_thread_pause_class::kNumberThreads});

        test_multi_thread_pause_class test;

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