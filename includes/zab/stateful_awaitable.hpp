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
 *  @file stateful_awaitable.hpp
 *
 */

#ifndef ZAB_STATEFUL_SUSPENSION_HPP_
#define ZAB_STATEFUL_SUSPENSION_HPP_

#include "zab/event.hpp"
#include "zab/generic_awaitable.hpp"

namespace zab {

    enum class notify_ctl {
        kSuspend,
        kResume,
        kReady
    };
    namespace details {

        template <typename Base, typename NotifyType>
        concept Notifiable = requires(Base a)
        {
            {
                a(std::declval<NotifyType>())
            }
            noexcept->std::same_as<notify_ctl>;
        };

        template <typename Base, typename NotifyType>
        concept HasStatefulSuspension = requires(Base a)
        {
            {
                a(std::declval<storage_event<NotifyType>*>())
            }
            noexcept;
        };

        template <typename Base, typename NotifyType>
        concept StatefulPassThroughSuspend = requires(Base a)
        {
            {
                a.await_suspend(std::declval<storage_event<NotifyType>*>())
            }
            noexcept;
        };

        template <typename Base, typename NotifyType>
        concept StatefulAwaitable =
            Notifiable<Base, NotifyType> && HasStatefulSuspension<Base, NotifyType>;

    }   // namespace details

    template <typename NotifyType, typename T>
    static constexpr bool
    is_stateful_suspend()
    {
        return std::is_convertible_v<T, storage_event<NotifyType>*>;
    }

    template <typename NotifyType, typename T>
    static constexpr bool
    is_notify()
    {
        return std::is_same_v<T, NotifyType>;
    }

    template <typename NotifyType, details::StatefulAwaitable<NotifyType> Functor>
    class stateful_awaitable : private generic_awaitable<Functor> {

        public:

            stateful_awaitable(const stateful_awaitable& _copy)
                : stateful_awaitable(_copy.functor())
            { }

            stateful_awaitable(stateful_awaitable&& _move) : stateful_awaitable(_move.functor()) { }

            stateful_awaitable(Functor* _functor)
                : generic_awaitable<Functor>(_functor),
                  context_(storage_event{
                      .handle_ = tagged_event{event<>{
                          .cb_ =
                              +[](void* _this)
                              {
                                  auto* self = static_cast<stateful_awaitable*>(_this);
                                  self->notify();
                              },
                          .context_ = this}},
                      .result_ = NotifyType{}})
            { }

            void
            notify()
            {
                notify_ctl result = (*generic_awaitable<Functor>::functor())(context_.result_);
                while (true)
                {
                    switch (result)
                    {
                        case notify_ctl::kSuspend:
                            using return_type = decltype(await_suspend());

                            if constexpr (std::is_void_v<return_type>)
                            {
                                await_suspend();
                                return;
                            }
                            else
                            {
                                if (generic_awaitable<Functor>::functor()->await_suspend(&context_))
                                {
                                    return;
                                }
                                else { result = notify_ctl::kResume; }
                            }

                            break;

                        case notify_ctl::kResume:
                            execute_event(underlying_);
                            return;

                        case notify_ctl::kReady:
                            if (!await_ready()) { result = notify_ctl::kSuspend; }
                            else { result = notify_ctl::kResume; }
                    }
                }
            }

            template <details::StatefulAwaitable<NotifyType> F = Functor>
            auto
            await_suspend(std::coroutine_handle<> _awaiter) noexcept
            {
                std::cout << "Setting tag " << this << "\n";
                underlying_ = tagged_event{_awaiter};
                return await_suspend();
            }

            template <details::StatefulAwaitable<NotifyType> F = Functor>
            auto
            await_suspend() noexcept
            {
                if constexpr (details::PassThroughSuspend<F>)
                {
                    using return_type =
                        decltype(generic_awaitable<Functor>::functor()->await_suspend(&context_));
                    if constexpr (std::is_void_v<return_type>)
                    {
                        generic_awaitable<Functor>::functor()->await_suspend(&context_);
                    }
                    else if constexpr (std::is_same_v<bool, return_type>)
                    {
                        return generic_awaitable<Functor>::functor()->await_suspend(&context_);
                    }
                    else
                    {
                        static_assert(
                            std::is_void_v<return_type> || std::is_same_v<bool, return_type>,
                            "stateful_awaitable await_suspend() or "
                            "operator()() does support non bool or void return types.");
                    }
                }
                else if constexpr (details::GenericSuspend<F>)
                {
                    using return_type =
                        decltype((*generic_awaitable<Functor>::functor())(&context_));

                    if constexpr (std::is_void_v<return_type>)
                    {
                        (*generic_awaitable<Functor>::functor())(&context_);
                    }
                    else if constexpr (std::is_same_v<bool, return_type>)
                    {
                        return (*generic_awaitable<Functor>::functor())(&context_);
                    }
                    else
                    {
                        static_assert(
                            std::is_void_v<return_type> || std::is_same_v<bool, return_type>,
                            "stateful_awaitable await_suspend() or "
                            "operator()() does support non bool or void return types.");
                    }
                }
                else
                {
                    static_assert(
                        details::PassThroughSuspend<F> || details::GenericSuspend<F>,
                        "No await_suspend(storage_event<NotifyType>*) or "
                        "operator()(storage_event<NotifyType>*) provided.");
                }
            }

            template <details::StatefulAwaitable<NotifyType> F = Functor>
            bool
            await_ready() noexcept
            {
                return generic_awaitable<Functor>::await_ready();
            }

            template <details::StatefulAwaitable<NotifyType> F = Functor>
            decltype(auto)
            await_resume() noexcept
            {
                return generic_awaitable<Functor>::await_resume();
            }

            template <typename F = Functor>
            void
            inline_await_suspend(tagged_event _event) noexcept
            {
                underlying_ = _event;
                if constexpr (std::is_void_v<decltype(await_suspend())>) { await_suspend(); }
                else
                {
                    auto result = await_suspend();

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

        private:

            storage_event<NotifyType> context_;
            tagged_event              underlying_;
    };

    template <typename NotifyType, details::StatefulAwaitable<NotifyType> Functor>
    auto
    stateful_suspension_point(Functor&& _functor) noexcept
    {
        return suspension_point<Functor, stateful_awaitable<NotifyType, Functor>>(
            std::forward<Functor>(_functor));
    }

}   // namespace zab

#endif /* ZAB_STATEFUL_SUSPENSION_HPP_ */
