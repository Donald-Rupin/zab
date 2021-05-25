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
 *  @file execution_promise.hpp
 *
 */

#ifndef ZAB_EXECUTION_PROMISE_HPP_
#define ZAB_EXECUTION_PROMISE_HPP_

#include <coroutine>
#include <iostream>

namespace zab {

    /**
     * @brief      This simplest promise type to allow a function
     *             to behave in asyncronous ways.
     */
    class execution_promise {

        public:

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            std::coroutine_handle<execution_promise>
            get_return_object() noexcept
            {
                return std::coroutine_handle<execution_promise>::from_promise(*this);
            }

            /**
             * @brief      All `execution_promise`'s' begin execution on function
             * call.
             *
             * @return     A `std::suspend_never`.
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_never{};
            }

            /**
             * @brief      When execution completes, the coroutine will clean
             * itself up.
             *
             * @return     `A std::suspend_never`.
             */
            auto
            final_suspend() noexcept
            {
                return std::suspend_never{};
            }

            /**
             * @brief      Returning is a no-op.
             */
            void
            return_void() noexcept
            { }

            /**
             * @brief      Exceptions are currently not implermented.
             */
            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }
    };

}   // namespace zab

#endif /* ZAB_EXECUTION_PROMISE_HPP_ */