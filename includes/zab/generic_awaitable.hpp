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
 *  @file generic_awaitable.hpp
 *
 */

#ifndef ZAB_GENERIC_AWAITABLE_HPP_
#define ZAB_GENERIC_AWAITABLE_HPP_

#include <coroutine>
#include <type_traits>

namespace zab {

    namespace details {

        template <typename Base, typename PromiseType>
        concept PassThroughSuspend = requires(Base a)
        {
            {
                a.await_suspend(std::coroutine_handle<PromiseType>{})
            }
            noexcept;
        };

        template <typename Base, typename PromiseType>
        concept GenericSuspend = requires(Base a)
        {
            {
                a(std::coroutine_handle<PromiseType>{})
            }
            noexcept;
        };

        struct ready_tag { };

        template <typename Base>
        concept PassThroughReady = requires(Base a)
        {
            {
                a.await_ready()
            }
            noexcept->std::same_as<bool>;
        };

        template <typename Base>
        concept GenericReady = requires(Base a)
        {
            {
                a(ready_tag{})
            }
            noexcept;
        };

        template <typename Base>
        concept ReadyReturnsBool = requires(Base a)
        {
            {
                a(ready_tag{})
            }
            noexcept->std::same_as<bool>;
        };

        struct resume_tag { };

        template <typename Base>
        concept PassThroughResume = requires(Base a)
        {
            {
                a.await_resume()
            }
            noexcept;
        };

        template <typename Base>
        concept GenericResume = requires(Base a)
        {
            {
                a(resume_tag{})
            }
            noexcept;
        };

    }   // namespace details

    template <typename T>
    static constexpr bool
    is_suspend()
    {
        return std::is_convertible_v<T, std::coroutine_handle<>>;
    }

    template <typename T>
    static constexpr bool
    is_ready()
    {
        return std::is_same_v<T, details::ready_tag>;
    }

    template <typename T>
    static constexpr bool
    is_resume()
    {
        return std::is_same_v<T, details::resume_tag>;
    }

    template <typename Functor>
    class generic_awaitable {

        public:

            generic_awaitable(const generic_awaitable& _copy) = default;

            generic_awaitable(generic_awaitable&& _move) = default;

            generic_awaitable&
            operator=(generic_awaitable&& _move_op) = default;

            generic_awaitable(Functor* _functor) : functor_(_functor) { }

            template <typename F = Functor, typename PromiseType>
            decltype(auto)
            await_suspend(std::coroutine_handle<PromiseType> _awaiter) noexcept
            {
                if constexpr (details::PassThroughSuspend<F, PromiseType>)
                {
                    return functor_->await_suspend(_awaiter);
                }
                else if constexpr (details::GenericSuspend<F, PromiseType>)
                {
                    return (*functor_)(_awaiter);
                }
                else
                {
                    static_assert(
                        details::PassThroughSuspend<F, PromiseType> ||
                            details::GenericSuspend<F, PromiseType>,
                        "No await_suspend(std::coroutine_handle<PromiseType>) or "
                        "operator()(std::coroutine_handle<PromiseType>) provided.");
                }
            }

            template <typename F = Functor>
            bool
            await_ready() noexcept
            {
                if constexpr (details::PassThroughReady<F>) { return functor_->await_ready(); }
                else if constexpr (details::GenericReady<F>)
                {
                    if constexpr (details::ReadyReturnsBool<decltype(*functor_)>)
                    {
                        return (*functor_)(details::ready_tag{});
                    }
                    else
                    {
                        (*functor_)(details::ready_tag{});
                        return false;
                    }
                }
                else
                {
                    static_assert(
                        details::PassThroughReady<F> || details::GenericReady<F>,
                        "No bool await_ready() or "
                        "[bool/void] operator()(details::ready_tag) provided.");

                    return false;
                }
            }

            template <typename F = Functor>
            decltype(auto)
            await_resume() noexcept
            {
                if constexpr (details::PassThroughResume<F>) { return functor_->await_resume(); }
                else if constexpr (details::GenericResume<F>)
                {
                    return (*functor_)(details::resume_tag{});
                }
                else
                {
                    static_assert(
                        details::GenericResume<F> || details::PassThroughResume<F>,
                        "No await_resume() or "
                        "operator()(details::resume_tag) provided.");
                }
            }

        private:

            Functor* functor_;
    };

    template <typename Functor>
    class co_awaitable {

        public:

            co_awaitable(const co_awaitable& _copy) = default;

            co_awaitable(co_awaitable&& _move) = default;

            co_awaitable&
            operator=(co_awaitable&& _move_op) = default;

            co_awaitable(Functor&& _functor) : functor_(std::move(_functor)) { }

            co_awaitable(const Functor& _functor) : functor_(_functor) { }

            template <typename... Args>
            co_awaitable(Args&&... _args) : functor_(std::forward<Args>(_args)...)
            { }

            auto operator co_await() noexcept { return generic_awaitable(&functor_); }

            auto
            inline_await() noexcept
            {
                return generic_awaitable(&functor_);
            }

        private:

            Functor functor_;
    };

}   // namespace zab

#endif /* ZAB_GENERIC_AWAITABLE_HPP_ */