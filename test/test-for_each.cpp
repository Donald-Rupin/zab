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
#include "zab/for_each.hpp"
#include "zab/reusable_future.hpp"
#include "zab/reusable_promise.hpp"
#include "zab/simple_promise.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_for_each();

    int
    run_test()
    {
        return test_for_each();
    }

    class test_for_each_class : public engine_enabled<test_for_each_class> {

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
                co_await for_each(
                    do_stuff(10),
                    [this, x = 0u](auto _opt) mutable noexcept -> void
                    {
                        if (expected(true, (bool) _opt)) { engine_->stop(); }

                        if (expected(x++, *_opt)) { engine_->stop(); }
                    });

                size_t amount = 0;
                co_await for_each(
                    do_stuff(100),
                    [this, x = 0u, &amount](auto _opt) mutable noexcept -> for_ctl
                    {
                        amount++;
                        if (expected(true, (bool) _opt)) { engine_->stop(); }

                        if (expected(x++, *_opt)) { engine_->stop(); }

                        if (x == 5) { return for_ctl::kBreak; }
                        else
                        {

                            return for_ctl::kContinue;
                        }
                    });

                if (expected(5u, amount)) { engine_->stop(); }

                co_await yield();

                failed_ = false;

                engine_->stop();
            }

            reusable_future<size_t>
            do_stuff(size_t _amount)
            {

                uint64_t count = 0;

                while (count < _amount)
                {

                    co_await yield();

                    co_yield count++;
                }

                co_return _amount;
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
    test_for_each()
    {
        engine engine(event_loop::configs{2});

        test_for_each_class test;

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