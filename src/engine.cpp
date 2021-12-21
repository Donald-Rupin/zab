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
 *  @file engine.cpp
 *
 */

#include "zab/engine.hpp"

#include "zab/async_function.hpp"
#include "zab/async_primitives.hpp"
#include "zab/signal_handler.hpp"
#include "zab/timer_service.hpp"

namespace zab {

    namespace {

        async_function<>
        do_function(engine* _this, std::function<void()> _func, order_t _order, thread_t _thread)
        {
            co_await yield(_this, _order, _thread);

            _func();
        }
    }   // namespace

    engine::engine(event_loop::configs _configs)
        : event_loop_(_configs), handler_(this), notifcations_(this),
          timer_(std::make_unique<timer_service>(this))
    { }

    engine::~engine()
    {
        timer_ = nullptr;

        event_loop_.purge();
        notifcations_.purge();
        event_loop_.purge();
    }

    void
    engine::execute(std::function<void()> _yielder, order_t _order, thread_t _thread) noexcept
    {
        do_function(this, std::move(_yielder), _order, _thread);
    }

    void
    engine::resume(event _handle, order_t _order, thread_t _thread) noexcept
    {
        if (!_order.order_) { event_loop_.send_event(_handle, _thread); }
        else if (timer_)
        {
            timer_->wait(_handle, _order.order_, _thread);
        }
        else
        {
            _handle.destroy();
        }
    }

}   // namespace zab