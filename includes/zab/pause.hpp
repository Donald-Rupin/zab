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
 *  @file pause.hpp
 *
 */

#ifndef ZAB_PAUSE_HPP_
#define ZAB_PAUSE_HPP_

#include <coroutine>
#include <utility>

#include "zab/generic_awaitable.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class engine;

    /**
     * @brief      Data pack for pausing coroutines.
     *
     */
    struct pause_pack {

            /**
             * @brief The thread to resume the coroutine in.
             *
             */
            thread_t thread_ = thread_t{};

            /**
             * @brief Any data that you wish to pass to the coroutine.
             *
             */
            std::intptr_t data_ = 0;

            /**
             * @brief The coroutine to resume.
             *
             */
            std::coroutine_handle<> handle_ = nullptr;
    };

    namespace details {

        /**
         * @brief Can the callable be nothrow called with a pause_pack*.
         *
         * @tparam Functor The callable type.
         */
        template <typename Functor>
        concept NoThrowInvoacablePP = std::is_nothrow_invocable_v<Functor, pause_pack*>;

    }   // namespace details

    /**
     * @brief Suspend a coroutine until unpause is called.
     *
     * @tparam Functor callable type.
     * @param _func The callable that is given the pause_pack* for resumption.
     * @co_return std::intptr_t The value of pause_pack->data_ given to the Functor.
     */
    template <details::NoThrowInvoacablePP Functor>
    inline auto
    pause(Functor&& _func) noexcept
    {
        return co_awaitable(
            [pp       = pause_pack{},
             function = std::forward<Functor>(_func)]<typename T>(T _handle) mutable noexcept
            {
                if constexpr (is_suspend<T>())
                {
                    pp.handle_ = _handle;
                    function(&pp);
                }
                else if constexpr (is_resume<T>())
                {
                    pause_pack ret = pp;
                    ret.handle_    = nullptr;
                    return ret;
                }
            });
    }

    /**
     * @brief      Continues a paused corountine.
     *
     * @param      _pause  The pause pack to resume.
     * @param[in]  _order  The ordering to apply.
     */
    void
    unpause(engine* _engine, pause_pack& _pause, order_t _order) noexcept;

    /**
     * @brief      Continues a paused corountine.
     *
     * @param      _pause  The pause pack to resume.
     */
    void
    unpause(engine* _engine, pause_pack& _pause) noexcept;

}   // namespace zab

#endif /* ZAB_PAUSE_HPP_ */
