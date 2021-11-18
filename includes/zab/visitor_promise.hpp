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
 *  @file visitor_promise.hpp
 *
 */

#ifndef ZAB_VISITOR_PROMISE_HPP_
#define ZAB_VISITOR_PROMISE_HPP_

#include "zab/defer_block_promise.hpp"
#include "zab/simple_future.hpp"

namespace zab {

    template <typename T>
    class visitor_promise;

    template <typename T = void>
    using visitor_future = simple_future<T, visitor_promise<T>>;

    template <typename T = void>
    class visitor_promise : public simple_promise<T> {

        public:

            auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<visitor_promise<T>>::from_promise(*this);
            }

            decltype(auto)
            data() noexcept
            {
                return forward(simple_promise<T>::data());
            }

            template <typename Q>
            decltype(auto)
            forward(Q&& _data)
            {
                if (defer_) { defer_(_data); }
                return std::forward<Q>(_data);
            }

            auto
            final_suspend() noexcept
            {
                return simple_promise<T>::final_suspend();
            }

            template <typename Functor>
                requires(std::is_nothrow_invocable_v<Functor, typename simple_promise<T>::returns&>)
            auto
            yield_value(defer_block<Functor>&& _block)
            {
                if (defer_)
                {

                    defer_ = [_block = std::move(_block.defered_), defered_ = std::move(defer_)](
                                 typename simple_promise<T>::returns& _value) mutable noexcept
                    {
                        _block(_value);
                        defered_(_value);
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

        private:

            std::function<void(typename simple_promise<T>::returns&)> defer_;
    };

    template <>
    class visitor_promise<void> : public simple_promise<void> {

        public:

            auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<visitor_promise<void>>::from_promise(*this);
            }

            auto
            final_suspend() noexcept
            {
                if (defer_) { defer_(); }
                return simple_promise<void>::final_suspend();
            }

            template <typename Functor>
                requires(std::is_nothrow_invocable_v<Functor>)
            auto
            yield_value(defer_block<Functor>&& _block)
            {
                if (defer_)
                {

                    defer_ = [_block   = std::move(_block.defered_),
                              defered_ = std::move(defer_)]() mutable noexcept
                    {
                        _block();
                        defered_();
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

        private:

            std::function<void(void)> defer_;
    };

}   // namespace zab

#endif