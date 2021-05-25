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
 *  @file test-visitor_promise.cpp
 *
 */

#include <cstdint>
#include <memory>
#include <optional>

#include "zab/async_latch.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/visitor_promise.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_visitor_class();

    int
    test_visit_complex_class();

    int
    run_test()
    {
        return test_visitor_class() || test_visit_complex_class();
    }

    class visit_void_class : public engine_enabled<visit_void_class> {

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
                co_await no_suspension();

                if (expected(1u, count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                co_await suspension();

                if (expected(2u, count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                co_await recursive();

                if (expected(4u, count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                co_await purge();

                if (expected(4u, count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                get_engine()->stop();
            }

            visitor_future<>
            no_suspension() noexcept
            {
                co_yield defer_block([this]() noexcept { ++count_; });
            }

            visitor_future<>
            suspension() noexcept
            {
                co_await yield();

                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();
            }

            visitor_future<>
            recursive() noexcept
            {
                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();

                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();
            }

            visitor_future<>
            purge() noexcept
            {
                co_yield defer_block([this]() noexcept { ++count_; });

                co_await yield();

                co_yield defer_block([this]() noexcept { ++count_; });

                co_yield purge_block{};
            }

            bool
            failed()
            {
                return count_ != 4;
            }

        private:

            size_t count_ = 0;
    };

    int
    test_visitor_class()
    {
        engine engine(event_loop::configs{});

        visit_void_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class visit_complex_class : public engine_enabled<visit_complex_class> {

        public:

            class complex {

                public:

                    complex() : data_(1000, 42) { }

                    complex(const complex& _copy) : code_(_copy.code_), data_(_copy.data_)
                    {
                        ++copy_count_;
                    }

                    complex(complex&& _move) : code_(_move.code_), data_(std::move(_move.data_))
                    {
                        ++move_count_;
                        _move.code_ = 0;
                    }

                    static void
                    clear()
                    {
                        copy_count_ = 0;
                        move_count_ = 0;
                    }

                    size_t
                    code()
                    {
                        return code_;
                    }

                    void
                    set_code(size_t _code)
                    {
                        code_ = _code;
                    }

                    static size_t copy_count_;

                    static size_t move_count_;

                private:

                    size_t                code_ = 0;
                    std::vector<std::uint16_t> data_;
            };

            static constexpr auto kDefaultThread = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                auto comp(co_await no_suspension(42u));

                if (expected(true, (bool) comp) && expected(42u, comp->code()))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* Must perfectly forward the complex objects... */
                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* 1 move in the return to the coroutine... */
                /* 1 move to the return in this function... */
                if (expected(2u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                complex::clear();

                auto comp2(co_await suspension(43u));

                if (expected(true, (bool) comp) && expected(43u, comp->code()))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* Must perfectly forward the complex objects... */
                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* 1 move in the return to the coroutine... */
                /* 1 move to the return in this function... */
                if (expected(2u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                complex::clear();

                auto comp3(co_await recursive(44u));

                if (expected(true, (bool) comp) && expected(44u, comp->code()))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* Must perfectly forward the complex objects... */
                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* 1 move in the return to the coroutine... */
                /* 1 move to the return in this function... */
                if (expected(2u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                complex::clear();

                auto comp4(co_await purge(45u));

                if (expected(true, (bool) comp) && expected(0u, comp->code()))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* Must perfectly forward the complex objects... */
                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                /* 1 move in the return to the coroutine... */
                /* 1 move to the return in this function... */
                if (expected(2u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return;
                }

                complex::clear();

                failed_ = false;
                get_engine()->stop();
            }

            visitor_future<complex>
            no_suspension(size_t code_) noexcept
            {
                complex comp;

                co_yield defer_block(
                    [this, code_](auto& _object) noexcept
                    {
                        if (_object) { _object->set_code(code_); }

                        if (expected(0u, complex::copy_count_))
                        {
                            get_engine()->stop();
                            return;
                        }

                        if (expected(1u, complex::move_count_))
                        {
                            get_engine()->stop();
                            return;
                        }
                    });

                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                if (expected(0u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                co_return comp;
            }

            visitor_future<complex>
            suspension(size_t code_) noexcept
            {
                complex comp;

                co_await yield();

                co_yield defer_block(
                    [this, code_](auto& _object) noexcept
                    {
                        if (_object) { _object->set_code(code_); }

                        if (expected(0u, complex::copy_count_))
                        {
                            get_engine()->stop();
                            return;
                        }

                        if (expected(1u, complex::move_count_))
                        {
                            get_engine()->stop();
                            return;
                        }
                    });

                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                if (expected(0u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                co_await yield();

                co_return comp;
            }

            visitor_future<complex>
            recursive(size_t code_) noexcept
            {
                complex comp;

                co_await yield();

                /* This defer block will override the next one... */
                co_yield defer_block(
                    [this, code_](auto& _object) noexcept
                    {
                        if (_object) { _object->set_code(code_); }

                        if (expected(0u, complex::copy_count_))
                        {
                            get_engine()->stop();
                            return;
                        }

                        if (expected(1u, complex::move_count_))
                        {
                            get_engine()->stop();
                            return;
                        }
                    });

                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                if (expected(0u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                co_await yield();

                co_yield defer_block(
                    [this](auto& _object) noexcept
                    {
                        if (_object) { _object->set_code(1000); }

                        if (expected(0u, complex::copy_count_))
                        {
                            get_engine()->stop();
                            return;
                        }

                        if (expected(1u, complex::move_count_))
                        {
                            get_engine()->stop();
                            return;
                        }
                    });

                co_return comp;
            }

            visitor_future<complex>
            purge(size_t code_) noexcept
            {
                complex comp;

                co_await yield();

                /* This defer block will override the next one... */
                co_yield defer_block(
                    [this, code_](auto& _object) noexcept
                    {
                        if (_object) { _object->set_code(code_); }

                        if (expected(0u, complex::copy_count_))
                        {
                            get_engine()->stop();
                            return;
                        }

                        if (expected(1u, complex::move_count_))
                        {
                            get_engine()->stop();
                            return;
                        }
                    });

                if (expected(0u, complex::copy_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                if (expected(0u, complex::move_count_))
                {
                    get_engine()->stop();
                    co_return std::nullopt;
                }

                co_await yield();

                co_yield defer_block(
                    [this](auto& _object) noexcept
                    {
                        if (_object) { _object->set_code(1000); }

                        if (expected(0u, complex::copy_count_))
                        {
                            get_engine()->stop();
                            return;
                        }

                        if (expected(1u, complex::move_count_))
                        {
                            get_engine()->stop();
                            return;
                        }
                    });

                co_yield purge_block{};

                co_return comp;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;
    };

    size_t visit_complex_class::complex::copy_count_ = 0;
    size_t visit_complex_class::complex::move_count_ = 0;

    int
    test_visit_complex_class()
    {
        engine engine(event_loop::configs{});

        visit_complex_class test;

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