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
 *  @file test-engine_enabled.cpp
 *
 */

#include <iostream>
#include <ostream>

#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_initialise();

    int
    test_main();

    /**
     * @brief      run all the tests.
     *
     * @return     0 if successful, not 0 otherwise
     */
    int
    run_test()
    {
        return test_initialise() || test_main();
    }

    class test_initialise_class : public engine_enabled<test_initialise_class> {

        public:

            bool
            initialised()
            {
                return initialised_;
            }

            void
            initialise() noexcept
            {
                initialised_ = true;

                std::cout << "Stopping!\n";
                engine_->stop();
            }

        private:

            bool initialised_ = false;
    };

    int
    test_initialise()
    {
        std::cout << "test_initialise\n";
        engine engine(engine::configs{1, engine::configs::kExact});

        test_initialise_class test;

        test.register_engine(engine);

        engine.start();

        return !test.initialised();
    }

    class test_main_class : public engine_enabled<test_main_class> {

        public:

            static constexpr auto kMainCadence = 0;
            static constexpr auto kMaxMains    = 5;

            size_t
            main_count()
            {
                return main_count_;
            }

            void
            main() noexcept
            {
                ++main_count_;
                if (main_count_ == kMaxMains)
                {
                    std::cout << "Stopping!\n";
                    engine_->stop();
                }
            }

        private:

            size_t main_count_ = 0;
    };

    int
    test_main()
    {
        std::cout << "test_main\n";
        engine engine(engine::configs{1, engine::configs::kExact});

        test_main_class test;

        test.register_engine(engine);

        engine.start();

        return test.main_count() != test_main_class::kMaxMains;
    }

}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}