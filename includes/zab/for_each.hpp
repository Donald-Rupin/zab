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
 *
 *  @file for_each.hpp
 *
 */

#ifndef ZAB_FOR_EACH_HPP_
#define ZAB_FOR_EACH_HPP_

#include <type_traits>

#include "zab/reusable_future.hpp"
#include "zab/simple_future.hpp"

namespace zab {

    /**
     * @brief      Controls for the for_each method, used to continue or break.
     */
    enum class for_ctl {
        kBreak,
        kContinue
    };

    /**
     * @brief      Iterate through results of the resuable_future giving the results to the
     * callback.
     *
     * @details    Resumes when the function signals completion.
     *
     * @param[in]  _reusable  The reusable future.
     * @param[in]  _functor   The functor give the results.
     *
     * @tparam     T          The template type for the reusable_future.
     * @tparam     P          The promise type for the reusable_future.
     * @tparam     Functor    The functor to use.
     */
    template <typename T, typename P, typename Functor>
        requires(
            (std::is_same_v<
                 std::result_of_t<Functor(typename reusable_future<T, P>::return_value)>,
                 void> ||
             std::is_same_v<
                 std::result_of_t<Functor(typename reusable_future<T, P>::return_value)>,
                 for_ctl>) )
    [[nodiscard]] simple_future<>
    for_each(reusable_future<T, P>&& _reusable, Functor&& _functor)
    {
        while (!_reusable.complete())
        {

            if constexpr (std::is_same_v<
                              std::result_of_t<Functor(
                                  typename reusable_future<T, P>::return_value)>,
                              void>)
            {

                std::forward<Functor>(_functor)(co_await _reusable);
            }
            else
            {

                auto result = std::forward<Functor>(_functor)(co_await _reusable);

                if (result == for_ctl::kBreak) { break; }
            }
        }
    }

}   // namespace zab

#endif
