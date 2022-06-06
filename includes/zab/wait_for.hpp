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
 *  @file wait_for.hpp
 *
 */

#ifndef ZAB_WAIT_FOR_HPP_
#define ZAB_WAIT_FOR_HPP_

#include <algorithm>
#include <atomic>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "zab/async_function.hpp"
#include "zab/async_latch.hpp"
#include "zab/engine.hpp"
#include "zab/event.hpp"
#include "zab/generic_awaitable.hpp"
#include "zab/reusable_future.hpp"
#include "zab/simple_future.hpp"
#include "zab/stateful_awaitable.hpp"
#include "zab/yield.hpp"

namespace zab {

    namespace details {

        template <typename Base>
        concept AwaitableVoid = requires(Base a)
        {
            {
                a.operator co_await().await_resume()
            }
            noexcept->std::same_as<void>;
        };

        template <typename Base>
        concept AwaitableProxy = !AwaitableVoid<Base> && requires(Base a)
        {
            {
                a.operator co_await()
            }
            noexcept;
        };

        /**
         * Helper for getting the type of concatanated tuples.
         */
        template <typename... input_t>
        using tuple_cat_t = decltype(std::tuple_cat(std::declval<input_t>()...));

        /**
         * @brief      Used for getting the return types of a series of futures.
         *
         * @details    This non-speicialised struct is undefined.
         *
         * @tparam     Promises  The pomises.
         */
        template <typename... Promises>
        struct extract_promise_types;

        template <AwaitableProxy Awaitable, typename... Promises>
        struct extract_promise_types<Awaitable, Promises...> {

                using type = std::decay_t<
                    decltype(std::declval<Awaitable>().operator co_await().await_resume())>;

                /**
                 * The concanated types of the font reusable_future and the remaining
                 * Futures.
                 */
                using types = tuple_cat_t<
                    std::tuple<type>,
                    typename extract_promise_types<Promises...>::types>;
        };

        template <AwaitableVoid Awaitable, typename... Promises>
        struct extract_promise_types<Awaitable, Promises...> {

                /**
                 * The concanated types of the font reusable_future and the remaining
                 * Futures.
                 */
                using types = tuple_cat_t<
                    std::tuple<promise_void>,
                    typename extract_promise_types<Promises...>::types>;
        };

        /**
         * @brief      Base case for recusion.
         */
        template <>
        struct extract_promise_types<> {
                using types = std::tuple<>;
        };

        template <typename... Promises, size_t... Is>
        auto
        inline_start_promise_imple(
            std::tuple<Promises...>& _promises,
            tagged_event             _event,
            std::index_sequence<Is...>) noexcept
        {
            std::initializer_list<int>({(std::get<Is>(_promises).inline_co_await(_event), 0)...});
        }

        template <typename... Promises>
        auto
        inline_start_promise(std::tuple<Promises...>& _promises, tagged_event _event) noexcept
        {
            return inline_start_promise_imple(
                _promises,
                _event,
                std::make_index_sequence<sizeof...(Promises)>{});
        }

        template <typename Result, typename Promise>
        auto
        do_get_inline_result(Result& _result, Promise& _promise) noexcept
        {
            if constexpr (std::is_same_v<decltype(_promise.get_inline_result()), void>)
            {
                // Still call in case it is expecting some form of reaping.
                _promise.get_inline_result();
            }
            else { _result = _promise.get_inline_result(); }

            return 0;
        }

        template <typename... Promises, typename... Results, size_t... Is>
        auto
        fill_in_results_imple(
            std::tuple<Promises...>& _promises,
            std::tuple<Results...>&  _results,
            std::index_sequence<Is...>) noexcept
        {
            std::initializer_list<int>(
                {do_get_inline_result(std::get<Is>(_results), std::get<Is>(_promises))...});
        }

        template <typename... Promises, typename... Results>
        auto
        fill_in_results(
            std::tuple<Promises...>& _promises,
            std::tuple<Results...>&  _results) noexcept
        {
            return fill_in_results_imple(
                _promises,
                _results,
                std::make_index_sequence<sizeof...(Promises)>{});
        }

    }   // namespace details

    /**
     * @brief      Given a variable amount of simple_promises, calls them in parallel and returns
     * the results after all have complete.
     *
     * @param      _engine   The engine to use for setting up.
     * @param[in]  _thread   The thread to return into.
     * @param      _args     The simple_promises
     *
     * @tparam     Promises  The types of the simple_promises.
     *
     * @return     A guaranteed_future that promises to get all of the results.
     */
    template <typename... Promises>
    auto
    wait_for(Promises&&... _args) noexcept
    {
        if constexpr (sizeof...(_args))
        {
            using result_type = typename details::extract_promise_types<Promises...>::types;
            using waiter_type = std::tuple<std::decay_t<Promises>...>;

            struct wait_for_data_pack {

                    wait_for_data_pack(waiter_type&& _wt)
                        : waiters_(std::move(_wt)), counter_(sizeof...(Promises) + 1)
                    { }

                    wait_for_data_pack(const wait_for_data_pack&) = delete;

                    wait_for_data_pack(wait_for_data_pack&& _wfdp)
                        : results_(std::move(_wfdp.results_)), waiters_(std::move(_wfdp.waiters_)),
                          counter_(sizeof...(Promises) + 1)
                    { }

                    ~wait_for_data_pack() = default;

                    result_type              results_;
                    waiter_type              waiters_;
                    std::atomic<std::size_t> counter_;

                    tagged_event event_;
            };

            return suspension_point(
                [wait_data = wait_for_data_pack(std::make_tuple(
                     std::forward<Promises>(_args)...))]<typename T>(T&& _handle) mutable noexcept
                {
                    if constexpr (is_ready<T>())
                    {
                        /* We are never ready... */
                        return false;
                    }
                    else if constexpr (is_suspend<T>())
                    {
                        wait_data.event_ = _handle;

                        details::inline_start_promise(
                            wait_data.waiters_,
                            event<>{
                                .cb_ =
                                    +[](void* _context)
                                    {
                                        auto* wait_data =
                                            static_cast<wait_for_data_pack*>(_context);

                                        if (wait_data->counter_.fetch_sub(
                                                1,
                                                std::memory_order_release) == 1)
                                        {
                                            execute_event(wait_data->event_);
                                        }
                                    },
                                .context_ = &wait_data});

                        if (wait_data.counter_.fetch_sub(1, std::memory_order_release) == 1)
                        {
                            return _handle;
                        }
                        else { return tagged_event{std::noop_coroutine()}; }
                    }
                    else
                    {
                        result_type rt;

                        details::fill_in_results(wait_data.waiters_, wait_data.results_);

                        rt.swap(wait_data.results_);

                        return rt;
                    }
                });
        }
    }

    template <typename T>
    guaranteed_future<std::vector<typename simple_future<T>::return_value>>
    wait_for(engine* _engine, std::vector<simple_future<T>>&& _args)
    {
        std::vector<simple_future<T>> futures;
        futures.swap(_args);

        std::vector<typename simple_future<T>::return_value> result(futures.size());

        /* Actually things to wait on */
        if (futures.size() > 0)
        {

            async_latch latch(_engine, futures.size() + 1);

            for (size_t count = 0; auto& future : futures)
            {
                [](auto& latch, auto& _result, auto& _future) noexcept -> async_function<>
                {
                    if constexpr (std::is_same_v<promise_void, std::decay_t<decltype(_result)>>)
                    {
                        co_await _future;
                    }
                    else { _result = co_await _future; }

                    latch.count_down();
                }(latch, result[count], future);

                ++count;
            }

            co_await latch.arrive_and_wait();
        }

        co_return result;
    }

}   // namespace zab

#endif /* ZAB_WAIT_FOR_HPP_ */
