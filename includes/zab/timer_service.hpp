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
 *  @file timer_service.hpp
 *
 */

#ifndef ZAB_TIMER_SERVICE_HPP_
#define ZAB_TIMER_SERVICE_HPP_

#include <coroutine>
#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "zab/async_function.hpp"
#include "zab/descriptor_notifications.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class engine;

    class timer_service {

        public:

            timer_service(engine* _engine);

            ~timer_service();

            void
            initialise() noexcept;

            void
            wait(std::coroutine_handle<> _handle, std::uint64_t _nano_seconds) noexcept;

            void
            wait(
                std::coroutine_handle<> _handle,
                std::uint64_t           _nano_seconds,
                thread_t                _thread) noexcept;

            struct await_proxy : public std::suspend_always {
                    void
                    await_suspend(std::coroutine_handle<> _awaiter) noexcept
                    {
                        ts_->wait(_awaiter, nano_seconds_);
                    }

                    timer_service* ts_;
                    std::uint64_t  nano_seconds_;
            };

            auto
            wait_proxy(std::uint64_t _nano_seconds) noexcept
            {
                struct {

                        await_proxy operator co_await() noexcept
                        {
                            return await_proxy{{}, ts_, nano_seconds_};
                        }

                        timer_service* ts_;
                        std::uint64_t  nano_seconds_;

                } co_awaiter{this, _nano_seconds};

                return co_awaiter;
            }

            simple_future<>
            wait_future(std::uint64_t _nano_seconds) noexcept
            {
                co_await wait_proxy(_nano_seconds);

                co_return;
            }

        private:

            async_function<>
            run();

            static constexpr auto kNanoInSeconds = 1000000000;

            engine* engine_;

            descriptor_notification::descriptor_waiter waiter_;

            std::uint64_t current_;

            std::mutex mtx_;
            std::map<std::uint64_t, std::vector<std::pair<std::coroutine_handle<>, thread_t>>>
                waiting_;
    };

}   // namespace zab

#endif /* ZAB_TIMER_SERVICE_HPP_ */