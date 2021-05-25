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

#include "zab/descriptor_notifications.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/signal_handler.hpp"

namespace zab {

    /**
     * @brief      This class describes an engine for enabling access to an
     *             interface and providing an event loop to execute requests.
     *
     */
    class engine {
        public:

            /**
             * @brief      Constructs an engine.
             */
            engine(event_loop::configs _configs);

            /**
             * @brief      Engines are movable.
             *
             * @param      _move  The engine to move
             */
            engine(engine&& _move) = default;

            
            /**
             * @brief      Destroys the engine.
             */
            ~engine();

            /**
             * @brief      Provides direct access to the signal handler.
             *
             * @return     The Wngines signal handler.
             */
            inline signal_handler&
            get_signal_handler() noexcept
            {
                return handler_;
            }

            inline descriptor_notification&
            get_notification_handler() noexcept
            {
                return *notifcations_;
            }

            /**
             * @brief      Provides direct access to the event loop.
             *
             * @return     The Wngines event loop.
             */
            inline event_loop&
            get_event_loop() noexcept
            {
                return event_loop_;
            }

            void
            execute(code_block&& _yielder, order_t _order, thread_t _thread) noexcept;

            void
            resume(coroutine _handle, order_t _order, thread_t _thread) noexcept;

            inline void
            start() noexcept
            {
                notifcations_->run();
                handler_.run();
                event_loop_.start();
            }

            inline void
            stop() noexcept
            {
                event_loop_.stop();
                handler_.stop();
                notifcations_->stop();
            }

        private:

            event_loop                               event_loop_;
            signal_handler                           handler_;
            std::unique_ptr<descriptor_notification> notifcations_;
    };

}   // namespace zab

#endif /* ZAB_ENGINE_HPP_ */
