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
 *  @file yield.hpp
 *
 */

#ifndef ZAB_YIELD_HPP_
#define ZAB_YIELD_HPP_

#include <utility>

#include "zab/engine.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    /**
     * @brief      Yields execution of the current coroutine.
     *
     * @param[in]  _engine  The engine to yield into.
     *
     * @return     A co_await'ble structure.
     */
    inline auto
    yield(engine* _engine)
    {
        struct YieldImpl {

                auto operator co_await() const noexcept
                {
                    struct {

                            void
                            await_suspend(std::coroutine_handle<> _awaiter) noexcept
                            {
                                yield_->engine_->resume(_awaiter);
                            }

                            bool
                            await_ready() const noexcept
                            {
                                return false;
                            }

                            void
                            await_resume() const noexcept
                            { }

                            const YieldImpl* yield_;

                    } yield_awaitable{.yield_ = this};

                    return yield_awaitable;
                }

                engine* engine_;

        } yield{.engine_ = _engine};

        return yield;
    }

    /**
     * @brief      Yields execution of the current coroutine.
     *
     * @param[in]  _engine  The engine to yield into.
     * @param[in]  _thread  The thread to resume in.
     *
     * @return     A co_await'ble structure.
     */
    inline auto
    yield(engine* _engine, thread_t _thread)
    {
        struct YieldImpl {

                auto operator co_await() const noexcept
                {
                    struct {

                            void
                            await_suspend(std::coroutine_handle<> _awaiter) noexcept
                            {
                                yield_->engine_->thread_resume(_awaiter, yield_->thread_);
                            }

                            bool
                            await_ready() const noexcept
                            {
                                return false;
                            }

                            void
                            await_resume() const noexcept
                            { }

                            const YieldImpl* yield_;

                    } yield_awaitable{.yield_ = this};

                    return yield_awaitable;
                }

                engine*  engine_;
                thread_t thread_;

        } yield{.engine_ = _engine, .thread_ = _thread};

        return yield;
    }

    /**
     * @brief      Yields execution of the current coroutine.
     *
     * @param[in]  _engine  The engine to yield into.
     * @param[in]  _order   The orderring to apply to the event loop.
     * @param[in]  _thread  The thread to resume in.
     *
     * @return     A co_await'ble structure.
     */
    inline auto
    yield(engine* _engine, order_t _order, thread_t _thread)
    {
        struct YieldImpl {

                auto operator co_await() const noexcept
                {
                    struct {

                            void
                            await_suspend(std::coroutine_handle<> _awaiter) noexcept
                            {
                                yield_->engine_->delayed_resume(
                                    _awaiter,
                                    yield_->order_,
                                    yield_->thread_);
                            }

                            bool
                            await_ready() const noexcept
                            {
                                return false;
                            }

                            void
                            await_resume() const noexcept
                            { }

                            const YieldImpl* yield_;

                    } yield_awaitable{.yield_ = this};

                    return yield_awaitable;
                }

                engine*  engine_;
                order_t  order_;
                thread_t thread_;

        } yield{.engine_ = _engine, .order_ = _order, .thread_ = _thread};

        return yield;
    }

}   // namespace zab

#endif /* ZAB_YIELD_HPP_ */
