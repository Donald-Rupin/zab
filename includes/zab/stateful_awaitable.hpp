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

    struct stateful_context {
            io_context              io_;
            std::coroutine_handle<> handle_;

            io_ptr
            get_io_ptr() noexcept
            {
                return create_io_ptr(&io_, kContextFlag);
            }
    };

    template <typename T>
    static constexpr bool
    is_stateful_suspend()
    {
        return std::is_same_v<T, stateful_context*>;
    }

    enum class notify_ctl {
        kSuspend,
        kResume,
        kReady
    };

    namespace details {

        template <typename Base>
        concept Notifiable = requires(Base a)
        {
            {
                a((int) 0)
            }
            noexcept->std::same_as<notify_ctl>;
        };

        template <typename Base>
        concept HasStatefulSuspension = requires(Base a)
        {
            {
                a(std::declval<stateful_context*>())
            }
            noexcept;
        };

        template <typename Base, typename PromiseType>
        concept StatefulPassThroughSuspend = requires(Base a)
        {
            {
                a.await_suspend(std::declval<stateful_context*>())
            }
            noexcept;
        };

        template <typename Base>
        concept StatefulAwaitable = Notifiable<Base> && HasStatefulSuspension<Base>;

    }   // namespace details

    template <typename T>
    static constexpr bool
    is_notify()
    {
        return std::is_same_v<T, int>;
    }

    template <details::StatefulAwaitable Functor>
    class stateful_awaitable : private generic_awaitable<Functor> {

        public:

            stateful_awaitable(const stateful_awaitable& _copy) = default;

            stateful_awaitable(stateful_awaitable&& _move) = default;

            stateful_awaitable&
            operator=(stateful_awaitable&& _move_op) = default;

            stateful_awaitable(Functor* _functor)
                : generic_awaitable<Functor>(_functor),
                  context_(
                      {.io_ =
                           io_context{
                               .cb_ =
                                   +[](void* _this, int _result)
                                   {
                                       stateful_awaitable* self =
                                           static_cast<stateful_awaitable*>(_this);
                                       self->notify(_result);
                                   },
                               .context_ = this},
                       .handle_ = nullptr})
            { }

            void
            notify(int _result)
            {
                notify_ctl result = (*generic_awaitable<Functor>::functor())(_result);
                switch (result)
                {
                    case notify_ctl::kSuspend:
                        await_suspend(context_.handle_);
                        break;

                    case notify_ctl::kResume:
                        context_.handle_.resume();
                        break;

                    case notify_ctl::kReady:
                        if (!await_ready()) { await_suspend(context_.handle_); }
                        else
                        {
                            context_.handle_.resume();
                        }
                        break;
                }
            }

            template <details::StatefulAwaitable F = Functor, typename PromiseType>
            decltype(auto)
            await_suspend(std::coroutine_handle<PromiseType> _awaiter) noexcept
            {
                context_.handle_ = _awaiter;
                if constexpr (details::StatefulPassThroughSuspend<F, PromiseType>)
                {
                    return generic_awaitable<Functor>::functor()->await_suspend(&context_);
                }
                else if constexpr (details::GenericSuspend<F, PromiseType>)
                {
                    return (*generic_awaitable<Functor>::functor())(&context_);
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

            template <details::StatefulAwaitable F = Functor>
            bool
            await_ready() noexcept
            {
                return generic_awaitable<Functor>::await_ready();
            }

            template <details::StatefulAwaitable F = Functor>
            decltype(auto)
            await_resume() noexcept
            {
                return generic_awaitable<Functor>::await_resume();
            }

        private:

            stateful_context context_;
    };

    template <typename Functor>
    using stateful_suspension_point = suspension_point<Functor, stateful_awaitable<Functor>>;

}   // namespace zab

#endif /* ZAB_STATEFUL_SUSPENSION_HPP_ */
