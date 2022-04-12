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
 *  @file signal_handler.hpp
 *
 */

#ifndef ZAB_SIGNAL_HANDLER_HPP_
#define ZAB_SIGNAL_HANDLER_HPP_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <signal.h>
#include <thread>
#include <vector>

#include "zab/async_function.hpp"
#include "zab/event_loop.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"
namespace zab {

    class engine;

    /**
     * @brief      This class describes a signal handler for re-entrant signals.
     *
     * @details    There can only be 1 signal_handler active at 1 time.
     *
     */
    class alignas(hardware_constructive_interference_size) signal_handler {

        public:

            static constexpr thread_t kSignalThread = thread_t{0};

            using handler = std::function<void(int)>;

            /**
             * @brief      initialise the handler with an engine to re-enter into.
             *
             * @param      _engine  The engine.
             *
             */
            signal_handler(engine* _engine);

            signal_handler(signal_handler&& _move) = default;

            /**
             * @brief      Destroys the signal_handler and resets the callbacks to
             * default.
             */
            ~signal_handler();

            /**
             * @brief      run the signal loop.
             */
            async_function<>
            run();

            /**
             * @brief      stop the signal loop.
             */
            void
            stop();

            /**
             * @brief      Register a handler for a given signal.
             *
             * @param[in]  _sig       The signal to handle.
             * @param[in]  _thread    The thread to run on.
             * @param      _function  The function to call.
             *
             * @return     If registereds successfully.
             */
            bool
            handle(int _sig, thread_t _thread, handler&& _function) noexcept;

            /**
             * @brief      Determines if signal_handler this active.
             *
             * @return     true if active, false otherwise.
             */
            bool
            is_active();

        private:

            std::unique_ptr<std::mutex>                              handlers_mtx_;
            std::map<int, std::vector<std::pair<thread_t, handler>>> handlers_;

            engine*    engine_;
            io_handle* handle_;

            int  fds_[2];
            bool running_ = false;
    };

}   // namespace zab

#endif /* ZAB_SIGNAL_HANDLER_HPP_ */