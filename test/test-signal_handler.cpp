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
 *  @file test-signal_handler.cpp
 *
 */

#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/signal_handler.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_raise();

    int
    run_test()
    {
        /* test twice to test double registering of handlers */
        return test_raise() || test_raise();
    }

    class test_raise_class : public engine_enabled<test_raise_class> {

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
                engine_->get_signal_handler().handle(
                    SIGUSR1,
                    default_thread(),
                    [this](int _s) noexcept
                    {
                        if (expected(engine_->current_id().thread_, kDefaultThread)) { return; }

                        if (expected(_s, SIGUSR1)) { return; }

                        handled_ = true;
                    });

                co_await yield();

                ::raise(SIGUSR1);

                /* Give some time for the handler to dispatch.  */
                /* 1 second is over kill but this sill may fail */
                /* on really slow/poorly scheduled systems      */
                co_await yield(order::in_seconds(1));

                if (handled_ == true) { failed_ = false; }

                engine_->stop();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool handled_ = false;

            bool failed_ = true;
    };

    int
    test_raise()
    {
        engine engine(engine::configs{.threads_ = 1, .opt_ = engine::configs::kExact});

        test_raise_class test;

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
