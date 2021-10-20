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
 *  @file event_loop.hpp
 *
 */

#ifndef ZAB_EVENT_LOOP_HPP_
#define ZAB_EVENT_LOOP_HPP_

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <new>
#include <ranges>
#include <thread>

#include "zab/event.hpp"
#include "zab/hardware_interface_size.hpp"
#include "zab/strong_types.hpp"
#include "zab/spin_lock.hpp"

namespace zab {

    /**
     * @brief      This class describes an multi threaded event loop. Events can
     * either target a specific thread or have a thread chosen for them using
     * `kAnyThread`.
     *
     *
     */
    class event_loop {

        public:

            struct configs {

                    enum thread_option
                    {
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
             * Constant to use when asking for any thread.
             */
            static constexpr thread_t kAnyThread = thread_t{};

            event_loop(configs _configs);

            /**
             * @brief      Default destroy the event loop and also destroy all queued co-routines.
             */
            ~event_loop();

            /**
             * @brief      Gets the number of cores for the device.
             *
             * @return     The number of cores.
             */
            static uint16_t
            core_count() noexcept;

            static uint16_t
            validate(configs& _configs);

            /**
             * @brief      Power up all threads in the event loop.
             *
             * @details    This function blocks until `stop()` is called.
             *             At the completion of this function, all threads are
             * joined.
             */
            void
            start() noexcept;

            /**
             * @brief      Requests all event loop threads to stop.
             *
             */
            void
            stop() noexcept;

            /**
             * @brief      Destroy the events loops explicitly.
             *
             * @details    This is used to purge out pending coroutines.
             */
            void
            purge() noexcept;

            /**
             * @brief      Sends an event to be executed by a event loop processor.
             *
             * @details    If _thread_number is large then ThreadCount this will
             * cause undefined behavior.
             *
             *             This is with the exception of being passed kAnyThread
             * which will select which thread to use.
             *
             * @param[in]  _event          The event
             * @param[in]  _thread_number  The thread number
             *D
             */
            void
            send_event(event _event, thread_t _thread_number) noexcept;

            /**
             * @brief      Sends an event to be executed by a event loop processor.
             *
             * @details    If _thread_number is large then ThreadCount this will
             * cause undefined behavior.
             *
             *             This is with the exception of being passed kAnyThread
             * which will select which thread to use.
             *
             * @param[in]  _event          The event
             *
             */
            void
            system_event(event _event) noexcept;

            /**
             * @brief      Get the ID of the thread running this function.
             *
             * @details    Returns kAnyThread if this thread is not an engine
             * thread.
             *
             * @return     The thread id.
             */
            thread_t
            current_id() const noexcept;

            /**
             * @brief      Get the number of worker events.
             *
             * @return     ThWDe nubmer of worker events.
             */
            inline uint16_t
            number_of_workers() const noexcept
            {
                return (uint16_t) workers_.size();
            }

        private:

            /**
             * @brief      Sets the affinity for the thread based on its ID.
             *
             * @param[in]  _thread_id  The thread identifier
             */
            void
            set_affinity(thread_t _thread_id) noexcept;

            /**
             * @brief      run the event loop for a specific thread.
             *
             * @param[in]  _stop_token     The stop token for the jthread.
             * @param[in]  _thread_number  The thread number.
             */
            void
            run_loop(std::stop_token _stop_token, thread_t _thread_number) noexcept;

           

            /* Todo: look at spin lock since ME zone is incredibly small */
            struct alignas(hardware_constructive_interference_size) safe_queue {

                    safe_queue();

                    safe_queue(safe_queue&& _que);

                    ~safe_queue();

                    std::atomic<std::size_t> size_ alignas(hardware_constructive_interference_size);

                    spin_lock mtx_ alignas(hardware_constructive_interference_size);

                    std::condition_variable_any cv_;

                    std::deque<event> events_;
            };

            std::vector<std::pair<safe_queue, std::jthread>>
                workers_ alignas(hardware_constructive_interference_size);

            std::vector<std::thread::id> ids_ alignas(hardware_constructive_interference_size);

            configs configs_ alignas(hardware_constructive_interference_size);
    };

}   // namespace zab

#endif /* ZAB_EVENT_LOOP_HPP_ */