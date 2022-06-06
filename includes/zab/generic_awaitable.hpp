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

#include "zab/event.hpp"

namespace zab {

    namespace details {

        template <typename Base>
        concept PassThroughSuspend = requires(Base a)
        {
            {
                a.await_suspend(std::declval<tagged_event>())
            }
            noexcept;
        };

        template <typename Base>
        concept GenericSuspend = requires(Base a)
        {
            {
                a(std::declval<tagged_event>())
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
        return std::is_convertible_v<T, tagged_event>;
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
            auto
            await_suspend(std::coroutine_handle<PromiseType> _awaiter) noexcept
            {
                return await_suspend(tagged_event{_awaiter});
            }

            template <typename F = Functor>
            auto
            await_suspend(tagged_event _event) noexcept
            {
                if constexpr (details::PassThroughSuspend<F>)
                {
                    if constexpr (std::is_void_v<decltype(functor_->await_suspend(_event))>)
                    {
                        functor_->await_suspend(_event);
                    }
                    else
                    {
                        auto result = functor_->await_suspend(_event);
                        return handle_suspend(result);
                    }
                }
                else if constexpr (details::GenericSuspend<F>)
                {
                    if constexpr (std::is_void_v<decltype((*functor_)(_event))>)
                    {
                        (*functor_)(_event);
                    }
                    else
                    {
                        auto result = (*functor_)(_event);
                        return handle_suspend(result);
                    }
                }
                else
                {
                    static_assert(
                        details::PassThroughSuspend<F> || details::GenericSuspend<F>,
                        "No await_suspend(std::coroutine_handle<PromiseType>) or "
                        "operator()(std::coroutine_handle<PromiseType>) provided.");
                }
            }

            template <typename F = Functor>
            void
            inline_await_suspend(tagged_event _event) noexcept
            {
                if constexpr (std::is_void_v<decltype(await_suspend(_event))>)
                {
                    await_suspend(_event);
                }
                else
                {
                    auto result = await_suspend(_event);

                    using result_t = decltype(result);

                    if constexpr (std::is_same_v<result_t, bool>)
                    {
                        if (!result) { execute_event(_event); }
                    }
                    else if constexpr (std::is_same_v<result_t, tagged_event>)
                    {
                        execute_event(result);
                    }
                    else if constexpr (std::is_convertible_v<result_t, std::coroutine_handle<>>)
                    {
                        auto h = (std::coroutine_handle<>) result;
                        h.resume();
                    }
                    else
                    {
                        static_assert(
                            !sizeof(result),
                            "Only return types of void, bool, std::coroutine_handle<P> and "
                            "tagged_event are supported by await_suspend");
                    }
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

            Functor*
            functor() const noexcept
            {
                return functor_;
            }

        protected:

            bool
            handle_suspend(bool _result) noexcept
            {
                return _result;
            }

            std::coroutine_handle<>
            handle_suspend(std::coroutine_handle<> _result) noexcept
            {
                return _result;
            }

            std::coroutine_handle<>
            handle_suspend(tagged_event _result) noexcept
            {
                std::coroutine_handle<> handle = std::noop_coroutine();

                std::visit(
                    [&handle]<typename T>(T _data)
                    {
                        if constexpr (std::is_same_v<T, std::coroutine_handle<>>)
                        {
                            handle = _data;
                        }
                        else { execute_event(_data); }
                    },
                    _result);

                return handle;
            }

        private:

            Functor* functor_;
    };

    template <typename Functor, typename AwaitableType = generic_awaitable<Functor>>
    class suspension_point {

        public:

            suspension_point(Functor&& _functor) : functor_(std::move(_functor)), at_(&functor_) { }

            suspension_point(const Functor& _functor) : functor_(_functor), at_(&functor_) { }

            template <typename... Args>
            suspension_point(Args&&... _args)
                : functor_(std::forward<Args>(_args)...), at_(&functor_)
            { }

            suspension_point(suspension_point&& _move)
                : functor_(std::move(_move.functor_)), at_(&functor_)
            { }

            suspension_point&
            operator=(suspension_point&& _move_op)
            {
                functor_ = std::move(_move_op.functor_);
                at_      = AwaitableType(&functor_);
            }

            auto operator co_await() noexcept { return at_; }

            void
            inline_co_await(tagged_event _event) noexcept
            {
                if (!at_.await_ready()) { at_.inline_await_suspend(_event); }
                else { execute_event(_event); }
            }

            decltype(auto)
            get_inline_result() noexcept
            {
                return at_.await_resume();
            }

        private:

            Functor       functor_;
            AwaitableType at_;
    };

}   // namespace zab

#endif /* ZAB_GENERIC_AWAITABLE_HPP_ */
