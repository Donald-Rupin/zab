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
 *  @file observable.hpp
 *
 */

#ifndef ZAB_OBERVABLE_HPP_
#define ZAB_OBERVABLE_HPP_

#include <coroutine>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/simple_future.hpp"

namespace zab {

    template <typename... Args>
    class observable : public engine_enabled<observable<Args...>> {

        public:

            using super = engine_enabled<observable<Args...>>;

            observable(engine* _engine) : mtx_(_engine) { super::register_engine(*_engine); }

            struct pending_result {
                    async_latch*         latch_ = nullptr;
                    std::tuple<Args...>* items_ = nullptr;
                    pending_result*      next_  = nullptr;
            };

            struct internal_observer {

                    internal_observer() : result_(nullptr) { }

                    ~internal_observer()
                    {
                        auto* tmp = result_;
                        while (tmp)
                        {
                            tmp->latch_->count_down();
                            tmp = tmp->next_;
                        }

                        if (handle_) { handle_.destroy(); }
                    }
                    std::mutex              mtx_;
                    pending_result*         result_;
                    std::coroutine_handle<> handle_;
                    thread_t                thread_;
            };

            class observer {

                    friend class observable;

                public:

                    observer(observable* _ob, internal_observer* _internal)
                        : observable_(_ob), internal_(_internal)
                    { }

                    observer(const observer&) = delete;

                    observer(observer&& _move)
                        : observable_(_move.observable_), internal_(_move.internal_)
                    {
                        _move.observable_ = nullptr;
                        _move.internal_   = nullptr;
                    }

                    observer&
                    operator=(observer&& _move_assign) noexcept
                    {
                        observable_              = _move_assign;
                        internal_                = _move_assign;
                        _move_assign.observable_ = nullptr;
                        _move_assign.internal_   = nullptr;
                    }

                    static zab::async_function<>
                    destroy(observer& _observer)
                    {
                        if (_observer.observable_)
                        {
                            co_await _observer.observable_->disconnect(_observer);
                        }
                    }

                    ~observer() { destroy(*this); }

                    class observer_guard {

                        public:

                            observer_guard(pending_result* _result) : result_(_result) { }

                            ~observer_guard() { result_->latch_->count_down(); }

                            const std::tuple<Args...>&
                            event() const noexcept
                            {
                                return *result_->items_;
                            }

                        private:

                            pending_result* result_;
                    };

                    auto operator co_await() noexcept
                    {
                        struct {
                                bool
                                await_suspend(std::coroutine_handle<> _awaiter) noexcept
                                {
                                    std::scoped_lock lck(ob_->internal_->mtx_);
                                    if (!ob_->internal_->result_)
                                    {
                                        ob_->internal_->handle_ = _awaiter;
                                        ob_->internal_->thread_ = thread_t{
                                            ob_->observable_->engine_->get_event_loop().current_id()

                                        };

                                        return true;
                                    }

                                    return false;
                                }

                                bool
                                await_ready() const noexcept
                                {
                                    return false;
                                }

                                [[nodiscard]] observer_guard
                                await_resume() const noexcept
                                {
                                    std::scoped_lock lck(ob_->internal_->mtx_);
                                    auto             tmp    = ob_->internal_->result_;
                                    ob_->internal_->result_ = ob_->internal_->result_->next_;
                                    return observer_guard{tmp};
                                }

                                observer* ob_;

                        } proxy{this};

                        return proxy;
                    }

                private:

                    observable*        observable_;
                    internal_observer* internal_;
            };

            template <typename... PArgs>
            [[nodiscard]] simple_future<>
            emit(PArgs&&... _args)
            {
                /* Store values to ensure life time through suspension... */
                std::tuple<Args...> data(std::forward<PArgs>(_args)...);

                std::deque<pending_result> results;
                std::optional<async_latch> latch;
                {
                    auto lck = co_await mtx_;

                    if (observers_.size())
                    {
                        latch.emplace(super::engine_, observers_.size() + 1);

                        for (auto& o : observers_)
                        {
                            results.emplace_back(pending_result{
                                .latch_ = &*latch,
                                .items_ = &data,
                                .next_  = nullptr});

                            std::scoped_lock lck(o->mtx_);

                            auto* new_res = &results.back();

                            if (!o->result_) { o->result_ = new_res; }
                            else
                            {
                                /* We do not expect there to be much */
                                /* So we will iterate instead of keeping head/tail */
                                auto* tmp = o->result_;

                                while (tmp->next_)
                                {
                                    tmp = tmp->next_;
                                }

                                tmp->next_ = new_res;
                            }

                            if (o->handle_)
                            {
                                auto tmp   = o->handle_;
                                o->handle_ = nullptr;
                                super::engine_->resume(
                                    tmp,
                                    order_t{order::now()},
                                    o->thread_);
                            }
                        }
                    }
                }

                if (latch)
                {
                    /* Block until all observers have read the result. */
                    co_await latch->arrive_and_wait();
                }
            }

            template <typename... PArgs>
            async_function<>
            async_emit(PArgs&&... _args)
            {
                co_await emit(std::forward<PArgs>(_args)...);
            }

            [[nodiscard]] guaranteed_future<observer>
            connect()
            {
                auto lck = co_await mtx_;

                observers_.emplace_back(std::make_unique<internal_observer>());

                co_return observer{this, observers_.back().get()};
            }

            [[nodiscard]] simple_future<>
            disconnect(observer& _observer)
            {
                if (_observer.internal_)
                {

                    /* edit state before suspend */
                    auto internal       = _observer.internal_;
                    _observer.internal_ = nullptr;

                    auto lck = co_await mtx_;

                    for (auto it = observers_.begin(); it != observers_.end(); ++it)
                    {
                        if (it->get() == internal)
                        {
                            observers_.erase(it);
                            break;
                        }
                    }
                }

                co_return;
            }

            [[nodiscard]] simple_future<>
            await_disconnect()
            {
                while (true)
                {
                    {
                        auto lck = co_await mtx_;

                        if (!observers_.size()) { co_return; }
                    }

                    co_await super::yield(order::in_milli(100));
                }
            }

        private:

            /* We use an async_mutex here to ensure linearity */
            /* Acquisition lock exhibits a strong */
            async_mutex                                    mtx_;
            std::deque<std::unique_ptr<internal_observer>> observers_;
    };

}   // namespace zab

#endif /* ZAB_OBERVABLE_HPP_ */