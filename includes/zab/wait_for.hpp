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

#ifndef ZAB_COMBINE_OVERLAY_HPP_
#define ZAB_COMBINE_OVERLAY_HPP_

#include <atomic>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <vector>

#include "zab/async_function.hpp"
#include "zab/async_latch.hpp"
#include "zab/async_primitives.hpp"
#include "zab/engine.hpp"
#include "zab/reusable_future.hpp"
#include "zab/simple_future.hpp"

namespace zab {

    namespace details {

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

        /**
         * @brief      Speicialisation for extract_promise_types for simple_futures.
         *
         * @tparam     ReturnType  The return type of the Future.
         * @tparam     Promises    The remaining simple_futures
         */
        template <typename ReturnType, typename PromiseType, typename... Promises>
        struct extract_promise_types<simple_future<ReturnType, PromiseType>, Promises...> {

                /**
                 * The concanated types of the font simple_future and the remaining
                 * Futures.
                 */
                using types = tuple_cat_t<
                    std::tuple<typename simple_future<ReturnType, PromiseType>::return_value>,
                    typename extract_promise_types<Promises...>::types>;
        };

        /**
         * @brief      Speicialisation for extract_promise_types for reusable_futures.
         *
         * @tparam     ReturnType  The return type of the Future.
         * @tparam     Promises    The remaining simple_futures
         */
        template <typename ReturnType, typename... Promises>
        struct extract_promise_types<reusable_future<ReturnType>, Promises...> {

                /**
                 * The concanated types of the font reusable_future and the remaining
                 * Futures.
                 */
                using types = tuple_cat_t<
                    std::tuple<typename reusable_future<ReturnType>::return_value>,
                    typename extract_promise_types<Promises...>::types>;
        };

        /**
         * @brief      Base case for recusion.
         */
        template <>
        struct extract_promise_types<> {
                using types = std::tuple<>;
        };

        /**
         * @brief      In order applies the elements from both tuples to the function.
         *
         * @param      _results   The first tuple.
         * @param      _promises  The second tuple.
         * @param[in]  function   The function to be applied.
         * @param[in]  <unnamed>  Index sequence for folding.
         *
         * @tparam     Results    The first tuple types.
         * @tparam     Promises   The second tuple types.
         * @tparam     Function   The tupe of the function.
         * @tparam     Is         The index sequence.
         *
         * @return     A tuple with the results of {f(t_1, t_2,), f(t_2, t_3),
         * ...}
         */
        template <typename... Results, typename... Promises, typename Function, size_t... Is>
        void
        init_wait_imple(
            std::tuple<Results...>&  _results,
            std::tuple<Promises...>& _promises,
            Function&                _function,
            std::index_sequence<Is...>)
        {
            std::initializer_list<int>(
                {_function(std::get<Is>(_results), std::get<Is>(_promises))...});
        }

        /**
         * @brief      In order applies the elements from both tuples to the function.
         *
         * @param      _results   The first tuple.
         * @param      _promises  The second tuple.
         * @param[in]  function   The function to be applied.
         *
         * @tparam     Results    The first tuple types.
         * @tparam     Promises   The second tuple types.
         * @tparam     Function   The type of the function.
         *
         * @return     A tuple with the results of {f(t_1, t_2,), f(t_2, t_3),
         * ...}
         */
        template <typename... Results, typename... Promises, typename Function>
        auto
        init_wait(
            std::tuple<Results...>&  _results,
            std::tuple<Promises...>& _promises,
            Function&&               _function)
        {
            return init_wait_imple(
                _results,
                _promises,
                _function,
                std::make_index_sequence<sizeof...(Results)>{});
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
    guaranteed_future<typename details::extract_promise_types<Promises...>::types>
    wait_for(engine* _engine, Promises&&... _args)
    {

        typename details::extract_promise_types<Promises...>::types result;

        /* Actually things to wait on */
        if constexpr (sizeof...(_args) > 0)
        {

            async_latch latch(_engine, sizeof...(_args) + 1);

            auto functions = std::make_tuple(std::reference_wrapper(_args)...);

            /* Convert promises to async_function<> the set the value and notify us
             * when all is complete */
            details::init_wait(
                result,
                functions,
                [&latch](auto& _result, auto& _future) noexcept
                {
                    [](auto& _latch, auto& _result, auto& _future) noexcept -> async_function<>
                    {
                        if constexpr (std::is_same_v<promise_void, std::decay_t<decltype(_result)>>)
                        {

                            co_await _future;
                        }
                        else
                        {

                            _result = co_await _future;
                        }

                        _latch.count_down();
                    }(latch, _result, _future);

                    /* Helps it fold... */
                    return 0;
                });

            co_await latch.arrive_and_wait();
        }

        co_return result;
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
                    else
                    {

                        _result = co_await _future;
                    }

                    latch.count_down();
                }(latch, result[count], future);

                ++count;
            }

            co_await latch.arrive_and_wait();
        }

        co_return result;
    }

}   // namespace zab

#endif /* ZAB_COMBINE_OVERLAY_HPP_ */
