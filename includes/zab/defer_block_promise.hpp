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
 *  @file defer_block_promise.hpp
 *
 */

#ifndef ZAB_DEFER_BLOCK_PROMISE
#define ZAB_DEFER_BLOCK_PROMISE

#include <functional>

namespace zab {

    template <typename T>
    struct defer_block {

            defer_block(T&& _defer) : defered_(_defer) { }

            T defered_;
    };

    struct purge_block { };

    class defer_block_promise {

        public:

            ~defer_block_promise()
            {
                /* In case we are explitelty destroyed */
                if (defer_) { defer_(); }
            }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<defer_block_promise>::from_promise(*this);
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

                if (defer_)
                {
                    defer_();
                    defer_ = nullptr;
                }

                return std::suspend_never{};
            }

            /**
             * @brief      Returning is a no-op.
             */
            void
            return_void() noexcept
            { }

            template <typename T>
                requires(std::is_nothrow_invocable_v<T>)
            auto
            yield_value(defer_block<T>&& _block)
            {
                if (defer_)
                {

                    defer_ = [_block = std::move(_block.defered_),
                              defer_ = std::move(defer_)]() mutable noexcept
                    {
                        _block();
                        defer_();
                    };
                }
                else
                {

                    defer_ = std::move(_block.defered_);
                }

                return std::suspend_never{};
            }

            auto
            yield_value(purge_block&&)
            {
                defer_ = nullptr;
                return std::suspend_never{};
            }

            /**
             * @brief      Exceptions are currently not implermented.
             */
            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

        private:

            std::function<void(void)> defer_;
    };

}   // namespace zab

#endif /* ZAB_DEFER_BLOCK_PROMISE */
