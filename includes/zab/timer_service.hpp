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
#include <vector>

#include "zab/async_function.hpp"
#include "zab/event_loop.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class engine;

    /**
     * @brief A timer_service suspends is used to resume suspended coroutines based off a monotonic
     *        clock.
     *
     */
    class timer_service {

        public:

            /**
             * @brief Construct a new timer service object with an engine to use for resumption.
             *
             * @param _engine The engine to used.
             */
            timer_service(engine* _engine);

            /**
             * @brief Copy constructed is deleted.
             *
             */
            timer_service(const timer_service&) = delete;

            /**
             * @brief Move construct a timer service taking ownership of the resources.
             *
             * @param _move The timer_service to move.
             */
            timer_service(timer_service&& _move);

            /**
             * @brief Destroy the timer service object cleaning up resources.
             *
             */
            ~timer_service();

            /**
             * @brief Pause the coroutine for _nano_seconds.
             *
             * @param _nano_seconds The amount of nanoseconds to pause for.
             * @co_return void Suspends for the given time.
             */
            auto
            wait(std::uint64_t _nano_seconds) noexcept
            {
                return co_awaitable(
                    [this, _nano_seconds]<typename T>(T _handle) noexcept
                    {
                        if constexpr (is_suspend<T>()) { return _nano_seconds == 0; }
                        else if constexpr (is_suspend<T>())
                        {
                            wait(_handle, _nano_seconds);
                        }
                    });
            }

            /**
             * @brief Pause the coroutine for _nano_seconds and resume in the given thread.
             *
             * @param _nano_seconds The amount of nanoseconds to pause for.
             * @param _thread The thread to resume in.
             * @co_return void Suspends for the given time.
             */
            auto
            wait(std::uint64_t _nano_seconds, thread_t _thread) noexcept
            {
                return co_awaitable(
                    [this, _nano_seconds, _thread]<typename T>(T _handle) noexcept
                    {
                        if constexpr (is_suspend<T>()) { return _nano_seconds == 0; }
                        else if constexpr (is_suspend<T>())
                        {
                            wait(_handle, _nano_seconds, _thread);
                        }
                    });
            }

            /**
             * @brief Takes a coroutine handle to resume after a given amount of time.
             *
             * @param _handle The handle to resume.
             * @param _nano_seconds The amount of nanoseconds to suspend for.
             */
            void
            wait(std::coroutine_handle<> _handle, std::uint64_t _nano_seconds) noexcept;

            /**
             * @brief Takes a coroutine handle to resume after a given amount of time and resume in
             *        the given thread
             *
             * @param _handle The handle to resume.
             * @param _nano_seconds The amount of nanoseconds to suspend for.
             * @param _thread The thread to resume in.
             */
            void
            wait(
                std::coroutine_handle<> _handle,
                std::uint64_t           _nano_seconds,
                thread_t                _thread) noexcept;

            [[deprecated("Use wait in favour of this function. This will be removed "
                         "once first_of and wait_for accept any awaitable.")]] simple_future<>
            wait_future(std::uint64_t _nano_seconds) noexcept
            {
                co_await wait(_nano_seconds);

                co_return;
            }

            /**
             * @brief Runs the background fibre that runes the timer services logic.
             *
             * @return async_function<>
             */
            async_function<>
            run() noexcept;

        private:

            void
            change_timer(std::uint64_t _nano_seconds) noexcept;

            static constexpr auto kNanoInSeconds = 1000000000;

            engine*    engine_;
            io_handle* handle_;

            std::map<std::uint64_t, std::vector<std::pair<std::coroutine_handle<>, thread_t>>>
                waiting_;

            std::size_t read_buffer_;

            std::uint64_t current_;

            int timer_fd_;
    };

}   // namespace zab

#endif /* ZAB_TIMER_SERVICE_HPP_ */