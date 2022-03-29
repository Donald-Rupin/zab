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
#include "zab/generic_awaitable.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    /**
     * @brief      Yields execution of the current coroutine.
     *
     * @param[in]  _engine  The engine to yield into.
     *
     * @co_return  void Will be resumed by the engine in the current thread.
     */
    inline auto
    yield(engine* _engine) noexcept
    {
        return co_awaitable(
            [_engine]<typename T>(T _handle) noexcept
            {
                if constexpr (is_suspend<T>()) { _engine->resume(_handle); }
            });
    }

    /**
     * @brief      Yields execution of the current coroutine.
     *
     * @param[in]  _engine  The engine to yield into.
     * @param[in]  _thread  The thread to resume in.
     *
     * @co_return  void Will be resumed by the engine in _thread.
     */
    inline auto
    yield(engine* _engine, thread_t _thread) noexcept
    {
        return co_awaitable(
            [_engine, _thread]<typename T>(T _handle) noexcept
            {
                if constexpr (is_suspend<T>()) { _engine->thread_resume(_handle, _thread); }
            });
    }

    /**
     * @brief      Yields execution of the current coroutine.
     *
     * @param[in]  _engine  The engine to yield into.
     * @param[in]  _order   The orderring to apply to the event loop.
     * @param[in]  _thread  The thread to resume in.
     *
     * @co_return  void Will be resumed by the engine in _thread after _order nanoseconds have
     *             passed.
     */
    inline auto
    yield(engine* _engine, order_t _order, thread_t _thread) noexcept
    {
        return co_awaitable(
            [_engine, _order, _thread]<typename T>(T _handle) noexcept
            {
                if constexpr (is_suspend<T>())
                {
                    _engine->delayed_resume(_handle, _order, _thread);
                }
            });
    }

}   // namespace zab

#endif /* ZAB_YIELD_HPP_ */
