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
 *  @file first_of.hpp
 *
 */

#ifndef ZAB_FIRTS_OF_HPP
#define ZAB_FIRTS_OF_HPP

#include <atomic>

#include "zab/async_primitives.hpp"
#include "zab/async_semaphore.hpp"
#include "zab/event.hpp"
#include "zab/strong_types.hpp"
#include "zab/wait_for.hpp"

namespace zab {

    namespace details {

        template <typename>
        struct one_of_these;

        template <typename... T>
        struct one_of_these<std::tuple<T...>> {
                using types = std::variant<T...>;
        };

        template <typename... Results, typename... Promises, typename Function, size_t... Is>
        void
        init_first_imple(
            std::variant<Results...>& _results,
            std::tuple<Promises...>&  _promises,
            Function&                 _function,
            std::index_sequence<Is...>)
        {
            std::initializer_list<int>({_function(_results, std::get<Is>(_promises))...});
        }

        template <typename... Results, typename... Promises, typename Function>
        auto
        init_first(
            std::variant<Results...>& _results,
            std::tuple<Promises...>&  _promises,
            Function&&                _function)
        {
            return init_first_imple(
                _results,
                _promises,
                _function,
                std::make_index_sequence<sizeof...(Results)>{});
        }
    }   // namespace details

    template <typename T>
    struct AwaitWrapper {

            AwaitWrapper(T& _wrapper) : wrapper_(_wrapper) { }

            auto operator co_await() const noexcept { return wrapper_.operator co_await(); }

            T& wrapper_;
    };

    template <typename... Promises>
    guaranteed_future<typename details::one_of_these<
        typename details::extract_promise_types<Promises...>::types>::types>
    first_of(engine* _engine, Promises&&... _args)
    {
        typename details::one_of_these<
            typename details::extract_promise_types<Promises...>::types>::types result;

        /* Actually things to wait on */
        if constexpr (sizeof...(_args) > 0)
        {

            auto resume_thread = _engine->current_id();

            co_await zab::pause(
                [&](auto _pp) noexcept
                {
                    auto handle    = std::make_shared<std::atomic<zab::pause_pack*>>(_pp);
                    _pp->thread_   = resume_thread;
                    auto functions = std::make_tuple(std::reference_wrapper(_args)...);

                    details::init_first(
                        result,
                        functions,
                        [&handle, _engine](auto& _result, auto& _future) noexcept
                        {
                            [](auto _handle, auto& _result, auto _future, auto _engine) noexcept
                                -> async_function<>
                            {
                                if constexpr (std::is_same_v<
                                                  void,
                                                  std::decay_t<decltype(co_await _future)>>)
                                {
                                    co_await _future;

                                    auto handle =
                                        _handle->exchange(nullptr, std::memory_order_acquire);
                                    if (handle)
                                    {
                                        _result = promise_void{};
                                        unpause(_engine, *handle, order::now());
                                    }
                                }
                                else
                                {

                                    auto res = co_await _future;

                                    auto handle =
                                        _handle->exchange(nullptr, std::memory_order_acquire);
                                    if (handle)
                                    {
                                        _result = std::move(res);
                                        unpause(_engine, *handle, order::now());
                                    }
                                }
                            }(handle, _result, std::move(_future), _engine);

                            /* Helps it fold... */
                            return 0;
                        });
                });
        }

        co_return result;
    }

}   // namespace zab

#endif /* ZAB_FIRTS_OF_HPP */