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
 *  @file engine.hpp
 *
 */

#ifndef ZAB_ENGINE_HPP_
#define ZAB_ENGINE_HPP_

#include <cstdint>
#include <thread>
#include <vector>

#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/signal_handler.hpp"
#include "zab/timer_service.hpp"

namespace zab {

    class timer_service;

    /**
     * @brief      This class describes an engine for enabling access to an
     *             interface and providing an event loop to execute requests.
     *
     */
    class engine {
        public:

            struct configs {

                    enum thread_option {
                        kAny,
                        kAtLeast,
                        kExact
                    };

                    uint16_t threads_ = 1;

                    thread_option opt_ = kAtLeast;

                    bool affinity_set_ = true;

                    uint16_t affinity_offset_ = 0;
            };

            /**
             * @brief      Constructs an engine.
             */
            engine(configs _configs);

            /**
             * @brief      Engines are movable.
             *
             * @param      _move  The engine to move
             */
            engine(engine&& _move) = default;

            /**
             * @brief      Destroys the engine.
             */
            ~engine() = default;

            /**
             * @brief      Gets the number of cores for the device.
             *
             * @return     The number of cores.
             */
            static uint16_t
            core_count() noexcept;

            static uint16_t
            validate(configs& _configs);

            void
            set_worker_affinity(thread_t _thread_id) noexcept;

            /**
             * @brief      Provides direct access to the signal handler.
             *
             * @return     The Wngines signal handler.
             */
            inline signal_handler&
            get_signal_handler() noexcept
            {
                return sig_handler_;
            }

            /**
             * @brief      Provides direct access to the event loop.
             *
             * @return     The Wngines event loop.
             */
            inline event_loop&
            get_event_loop() noexcept
            {
                return get_event_loop(current_id());
            }

            inline event_loop&
            get_event_loop(thread_t _thread) noexcept
            {
                assert(_thread < event_loop_.size());
                return event_loop_[_thread.thread_];
            }

            inline timer_service&
            get_timer() noexcept
            {
                return get_timer(current_id());
            }

            inline timer_service&
            get_timer(thread_t _thread) noexcept
            {
                assert(_thread < timers_.size());
                return timers_[_thread.thread_];
            }

            void
            execute(std::function<void()> _yielder, order_t _order, thread_t _thread) noexcept;

            void
            resume(event _handle) noexcept;

            void
            thread_resume(event _handle, thread_t _thread) noexcept;

            void
            delayed_resume(event _handle, order_t _order) noexcept;

            void
            delayed_resume(event _handle, order_t _order, thread_t _thread) noexcept;

            void
            start() noexcept;

            void
            stop() noexcept;

            /**
             * @brief      Get the ID of the thread running this function.
             *
             * @details    Returns kAnyThread if this thread is not an engine
             * thread.
             *
             * @return     The thread id.
             */
            inline static thread_t
            current_id() noexcept
            {
                return this_thead_;
            }

            /**
             * @brief      Get the number of worker events.
             *
             * @return     ThWDe nubmer of worker events.
             */
            inline uint16_t
            number_of_workers() const noexcept
            {
                return event_loop_.size();
            }

        private:

            static thread_local thread_t this_thead_;

            thread_t
            get_any_thread();

            // This is mainly stop helgrind et al. complaining
            // The auto latch should stop any race conditions...
            std::mutex                 mtx_;
            std::vector<event_loop>    event_loop_;
            std::vector<timer_service> timers_;

            signal_handler sig_handler_;

            std::vector<std::jthread> threads_;

            configs configs_;
    };

}   // namespace zab

#endif /* ZAB_ENGINE_HPP_ */
