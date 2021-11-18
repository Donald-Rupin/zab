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
 *  @file test-wait_for.cpp
 *
 */

#include <cstdint>
#include <ranges>

#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/simple_promise.hpp"
#include "zab/strong_types.hpp"
#include "zab/wait_for.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_wait_for_tuple();
    int
    test_wait_for_vector();

    int
    run_test()
    {
        return test_wait_for_tuple() || test_wait_for_vector();
    }

    static constexpr std::uint16_t kNumberOfThreads = 5;

    class test_wait_for_tuple_class : public engine_enabled<test_wait_for_tuple_class> {

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
                failed_ =
                    !(co_await constants() && co_await same_length(1) && co_await same_length(5) &&
                      co_await same_length(12) && co_await variable_length(1) &&
                      co_await variable_length(5) && co_await variable_length(16) &&
                      co_await variable_length(123) && co_await recursive(1) &&
                      co_await recursive(7) && co_await recursive(14) && co_await recursive(58) &&
                      co_await recursive(1209));

                get_engine()->stop();
            }

            simple_future<bool>
            constants() noexcept
            {
                Reset();

                /* test constant void */
                co_await wait_for(void_promise(0));

                /* test constant bool */
                auto [result_b_constant] = co_await wait_for(BoolPromise(0));

                if (expected(0, result_b_constant)) { co_return false; }

                /* test constant size */
                auto [result_s_constant] = co_await wait_for(typed_promise(0));

                if (expected(0u, result_s_constant)) { co_return false; }

                /* test combination  */
                auto result_c_constant = co_await wait_for(
                    void_promise(0),
                    typed_promise(0),
                    BoolPromise(0),
                    typed_promise(0));

                if (expected(result_c_constant, std::make_tuple(promise_void{}, 0, false, 0)))
                {
                    co_return false;
                }

                if (expected(a_count_.load(), 0u) || expected(b_count_.load(), 0u) ||
                    expected(c_count_.load(), 0u))
                {
                    co_return false;
                }

                co_return true;
            }

            simple_future<bool>
            same_length(size_t _number_loops) noexcept
            {
                Reset();

                /* test constant void */
                co_await wait_for(void_promise(_number_loops));

                /* test constant bool */
                auto [result_b_constant] = co_await wait_for(BoolPromise(_number_loops));

                if (expected(_number_loops & 0b1, result_b_constant)) { co_return false; }

                /* test constant size */
                auto [result_s_constant] = co_await wait_for(typed_promise(_number_loops));

                if (expected(_number_loops, result_s_constant)) { co_return false; }

                /* test combination  */
                auto result_c_constant = co_await wait_for(
                    void_promise(_number_loops),
                    typed_promise(_number_loops),
                    BoolPromise(_number_loops),
                    typed_promise(_number_loops));

                if (expected(
                        result_c_constant,
                        std::make_tuple(
                            promise_void{},
                            _number_loops,
                            _number_loops & 0b1,
                            _number_loops)))
                {
                    co_return false;
                }

                if (expected(a_count_.load(), _number_loops * 2) ||
                    expected(b_count_.load(), _number_loops * 2) ||
                    expected(c_count_.load(), _number_loops * 3))
                {
                    co_return false;
                }

                co_return true;
            }

            simple_future<bool>
            variable_length(size_t _number_loops) noexcept
            {
                Reset();

                /* test combination  */
                auto result_c_constant = co_await wait_for(
                    void_promise(_number_loops / 4),
                    typed_promise(_number_loops * 2),
                    BoolPromise(_number_loops - 1),
                    typed_promise(_number_loops / 3));

                if (expected(
                        result_c_constant,
                        std::make_tuple(
                            promise_void{},
                            _number_loops * 2,
                            (_number_loops - 1) & 0b1,
                            (_number_loops / 3))))
                {
                    co_return false;
                }

                if (expected(a_count_.load(), _number_loops / 4) ||
                    expected(b_count_.load(), _number_loops - 1) ||
                    expected(c_count_.load(), _number_loops * 2 + _number_loops / 3))
                {
                    co_return false;
                }

                co_return true;
            }

            simple_future<bool>
            recursive(size_t _number_loops) noexcept
            {
                Reset();

                auto result_c_constant = co_await wait_for(
                    void_promise(_number_loops / 2),
                    wait_for(
                        void_promise(_number_loops * 2),
                        BoolPromise(_number_loops),
                        typed_promise(_number_loops - 1)),
                    wait_for(
                        wait_for(
                            typed_promise(_number_loops * 2),
                            void_promise(_number_loops),
                            BoolPromise(_number_loops),
                            typed_promise(_number_loops / 3)),
                        void_promise(_number_loops)));

                if (expected(
                        result_c_constant,
                        std::make_tuple(
                            promise_void{},
                            std::make_tuple(promise_void{}, _number_loops & 0b1, _number_loops - 1),
                            std::make_tuple(
                                std::make_tuple(
                                    _number_loops * 2,
                                    promise_void{},
                                    _number_loops & 0b1,
                                    _number_loops / 3),
                                promise_void{}))))
                {
                    co_return false;
                }

                if (expected(a_count_.load(), _number_loops / 2 + _number_loops * 4) ||
                    expected(b_count_.load(), _number_loops * 2) ||
                    expected(
                        c_count_.load(),
                        _number_loops * 2 + _number_loops / 3 + _number_loops - 1))
                {
                    co_return false;
                }

                co_return true;
            }

            simple_future<>
            void_promise(size_t _loops) noexcept
            {
                for (size_t i = 0; i < _loops; i++)
                {
                    ++a_count_;
                    co_await yield(now(), thread_t{});
                }
            }

            simple_future<bool>
            BoolPromise(size_t _loops) noexcept
            {
                for (size_t i = 0; i < _loops; i++)
                {
                    ++b_count_;
                    co_await yield(now(), thread_t{});
                }

                co_return _loops & 0b1;
            }

            guaranteed_future<size_t>
            typed_promise(size_t _loops) noexcept
            {
                for (size_t i = 0; i < _loops; i++)
                {
                    ++c_count_;
                    co_await yield(now(), thread_t{});
                }

                co_return _loops;
            }

            void
            Reset()
            {
                a_count_ = 0;

                b_count_ = 0;

                c_count_ = 0;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            std::atomic<size_t> a_count_ = 0;

            std::atomic<size_t> b_count_ = 0;

            std::atomic<size_t> c_count_ = 0;

            bool failed_ = true;
    };

    int
    test_wait_for_tuple()
    {
        engine engine(event_loop::configs{kNumberOfThreads, event_loop::configs::kAtLeast});

        test_wait_for_tuple_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_wait_for_vector_class : public engine_enabled<test_wait_for_vector_class> {

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
                failed_ =
                    !(co_await constants() && co_await same_length(1) && co_await same_length(5) &&
                      co_await same_length(12) && co_await variable_length(1) &&
                      co_await variable_length(5) && co_await variable_length(16) &&
                      co_await variable_length(123));

                get_engine()->stop();
            }

            simple_future<bool>
            constants() noexcept
            {
                std::vector<guaranteed_future<size_t>> vec;
                vec.emplace_back(typed_promise(0));

                // /* test constant size */
                auto result_s_constant = co_await wait_for(std::move(vec));

                if (expected(result_s_constant[0], 0u)) { co_return false; }

                co_return true;
            }

            simple_future<bool>
            same_length(size_t _number_loops) noexcept
            {
                std::vector<guaranteed_future<size_t>> vec;
                vec.emplace_back(typed_promise(_number_loops));

                /* test constant size */
                auto result_s_constant = co_await wait_for(std::move(vec));

                if (expected(_number_loops, result_s_constant[0])) { co_return false; }

                for (size_t i : std::ranges::views::iota(0u, _number_loops))
                {
                    (void) i;
                    vec.emplace_back(typed_promise(_number_loops));
                }

                /* test combination  */
                auto result_c = co_await wait_for(std::move(vec));

                for (size_t i : result_c)
                {
                    if (expected(i, _number_loops)) { co_return false; }
                }

                co_return true;
            }

            simple_future<bool>
            variable_length(size_t _number_loops) noexcept
            {
                std::vector<guaranteed_future<size_t>> vec;

                for (size_t i : std::ranges::views::iota(0u, _number_loops))
                {
                    (void) i;
                    vec.emplace_back(typed_promise(_number_loops * i));
                }

                /* test combination  */
                auto result_c = co_await wait_for(std::move(vec));

                for (size_t i : std::ranges::views::iota(0u, _number_loops))
                {
                    if (expected(result_c[i], _number_loops * i)) { co_return false; }
                }

                co_return true;
            }

            guaranteed_future<size_t>
            typed_promise(size_t _loops) noexcept
            {
                for (size_t i = 0; i < _loops; i++)
                {
                    co_await yield(now(), thread_t{});
                }

                co_return _loops;
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
    test_wait_for_vector()
    {
        engine engine(event_loop::configs{kNumberOfThreads, event_loop::configs::kAtLeast});

        test_wait_for_vector_class test;

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