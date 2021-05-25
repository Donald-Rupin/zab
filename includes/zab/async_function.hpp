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
 *  @file async_function.hpp
 *
 */

#ifndef ZAB_ASYNC_FUNCTION_HPP_
#define ZAB_ASYNC_FUNCTION_HPP_

#include <coroutine>

#include "zab/execution_promise.hpp"

namespace zab {

    namespace details {

        template <typename Base>
        concept Execution = requires(Base a)
        {
            {
                a.initial_suspend()
            }
            noexcept->std::same_as<std::suspend_never>;
            {
                a.final_suspend()
            }
            noexcept->std::same_as<std::suspend_never>;
            {
                a.return_void()
            }
            noexcept->std::same_as<void>;
            {
                a.get_return_object()
            }
            noexcept->std::same_as<std::coroutine_handle<Base>>;
        };

    }   // namespace details

    /**
     * @brief      This class describes an asynchronous function.
     *
     * @details    This classed should only be used as the return type of a
     *             function to enable coroutines. It should never be stored and only kept as
     *             a tempory object.
     *
     *             For example:
     *             ```
     *             auto foo() -> async_function { async behaviour };
     *
     *             void bar()
     *             {
     *              ...
     *              foo();
     *              ...
     *             }
     *
     */
    template <details::Execution PromiseType = execution_promise>
    class async_function {

        public:

            using promise_type = PromiseType;

            /* The handle for the coroutine */
            using coro_handle = std::coroutine_handle<promise_type>;

            /**
             * @brief Constructor no-ops.
             *
             * @param[in]  <unnamed>
             *
             */
            async_function(coro_handle){};

            /**
             * @brief      Default clean this object up.
             */
            ~async_function() = default;

            /**
             * @brief      Deleted copy constructor.
             *
             * @param[in]  <unnamed>
             *
             */
            async_function(const async_function&) = delete;

            /**
             * @brief      Deleted move constructor.
             *
             * @param[in]  <unnamed>
             *
             */
            async_function(async_function&&) = delete;

            /**
             * @brief      Deleted Assignment operator.
             *
             * @param[in]  <unnamed>
             *
             * @return     `this`
             */
            async_function&
            operator=(const async_function&) = delete;
    };

}   // namespace zab

#endif /* ZAB_ASYNC_FUNCTION_HPP_ */