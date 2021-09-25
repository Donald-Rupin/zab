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
 *  @file descriptor_notifications.hpp
 *
 */

#ifndef ZAB_DESCRIPTOR_NOTIFCATIONS_HPP_
#define ZAB_DESCRIPTOR_NOTIFCATIONS_HPP_

#include <algorithm>
#include <coroutine>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <sys/epoll.h>
#include <thread>

#include "zab/event.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class engine;

    /**
     * @brief      This class implements an asynchronous `epoll` based descriptor notification
     * service.
     */
    class descriptor_notification {

        public:

            struct waiting_coroutine {
                    std::coroutine_handle<> handle_;
                    thread_t                thread_;
            };

        private:

            struct timer_event {
                    std::uintptr_t context_        = 0;
                    std::int64_t   time_to_expiry_ = -1;
                    timer_event*   next_           = nullptr;
            };

            /**
             * @brief      This class is a for a descriptor, related information and the
             * callback information.
             */
            class descriptor {
                    friend class descriptor_notification;

                public:

                    /**
                     * @brief      Construct in an empty state.
                     */
                    descriptor();

                    /**
                     * @brief      Destroys the object. This is a non-owning object.
                     */
                    ~descriptor() = default;

                    /**
                     * @brief      Cannot be copied.
                     *
                     * @param[in]  <unnamed>
                     */
                    descriptor(const descriptor&) = delete;

                    /**
                     * @brief      The flags set by the service.
                     *
                     * @return     The flags.
                     */
                    inline int
                    return_flags() const noexcept
                    {
                        return return_flags_;
                    }

                    /**
                     * @brief      Sets the coroutine handle.
                     *
                     * @param[in]  _handle  The coroutine handle.
                     */
                    void
                    set_handle(std::coroutine_handle<> _handle) noexcept;


                    inline timer_event*
                    get_timeout() noexcept
                    {
                        return &timeout_;
                    }

                private:

                    timer_event        timeout_;
                    std::atomic<void*> awaiter_;
                    descriptor*        dead_chain_;
                    descriptor*        live_chain_;
                    int                return_flags_ = 0;
                    thread_t           thread_;
            };

        public:

            /**
             * @brief      Constructs a new instance that will register to this engine.
             *
             * @param      _engine  The engine.
             *
             */
            descriptor_notification(engine* _engine);

            /**
             * @brief      Destroys the object and cleans up the resources.
             */
            ~descriptor_notification();

            /**
             * @brief      Convince types for the epoll macro equivalent.
             */
            enum NoticationType
            {
                kError     = EPOLLERR,
                kRead      = EPOLLIN,
                kWrite     = EPOLLOUT,
                kException = EPOLLPRI,
                kClosed    = EPOLLRDHUP,
                kDestruction,
            };

            /**
             * @brief      This class describes a descriptor waiter used of co_waiting descriptor
             * events.
             *
             */
            class descriptor_waiter {

                public:

                    /**
                     * @brief      Constructs a new instance in an empty state.
                     */
                    descriptor_waiter();

                    /**
                     * @brief      Constructs a new instance registered to the
                     * descriptor_notification service and subscribed to _fd..
                     *
                     * @param      _self  The self
                     * @param      _desc  The description
                     * @param[in]  _fd    The file descriptor.
                     */
                    descriptor_waiter(descriptor_notification* _self, descriptor* _desc, int _fd);

                    /**
                     * @brief      Cannot copy this object.
                     *
                     * @param[in]  _copy  The copy
                     */
                    descriptor_waiter(const descriptor_waiter& _copy) = delete;

                    /**
                     * @brief      Constructs a new instance leave the old instance in an empty
                     * state.
                     *
                     * @param      _move  The move
                     */
                    descriptor_waiter(descriptor_waiter&& _move);

                    /**
                     * @brief      Swap two descriptor_waiter's.
                     *
                     * @param      _first   The first
                     * @param      _second  The second
                     */
                    friend void
                    swap(descriptor_waiter& _first, descriptor_waiter& _second) noexcept;

                    /**
                     * @brief      Destroys the object and unsubscribes the file descriptor from the
                     *             notification service.
                     */
                    ~descriptor_waiter();

                    /**
                     * @brief      Move assignment operator.
                     *
                     * @param      _move  The descriptor_waiter to move
                     *
                     * @return     The result of the assignment
                     */
                    descriptor_waiter&
                    operator=(descriptor_waiter&& _move);

                    /**
                     * @brief      The Awaitable Proxy used to co_await for events.
                     */
                    struct await_proxy {

                            /**
                             * @brief      Suspend an wait for the service to deliver an event.
                             *
                             * @param[in]  _awaiter  The coroutine handle.
                             */
                            void
                            await_suspend(std::coroutine_handle<> _awaiter) noexcept;

                            /**
                             * @brief      Always suspend.
                             *
                             * @return     false;
                             */
                            bool
                            await_ready() const noexcept
                            {
                                return false;
                            }

                            /**
                             * @brief      Return the return flags on resumption.
                             *
                             * @return     The return flags.
                             */
                            int
                            await_resume() const noexcept
                            {
                                return this_->return_flags();
                            };

                            descriptor_waiter* this_;
                    };

                    /**
                     * @brief      Sets the flags to watch for.
                     *
                     * @param[in]  _flags  The flags.
                     */
                    inline void
                    set_flags(int _flags) noexcept
                    {
                        flags_ = _flags;
                    }

                    /**
                     * @brief      Returns flags set by the service.
                     *
                     * @return     The return flags.
                     */
                    inline int
                    return_flags() const noexcept
                    {
                        return desc_->return_flags();
                    }

                    /**
                     * @brief      Gets the file descriptor.
                     *
                     * @return     The file descriptor.
                     */
                    inline int
                    file_descriptor() const noexcept
                    {
                        return fd_;
                    }

                    /**
                     * @brief      Sets the timeout.
                     *
                     * @param[in]  _timeout  The timeout
                     */
                    inline void
                    set_timeout(int32_t _timeout) noexcept
                    {
                        timeout_ = _timeout;
                    }

                    /**
                     * @brief      Wakes any co_waiting instances that have finished suspending.
                     */
                    void
                    wake_up() noexcept;

                    /**
                     * @brief      Co_await conversion operator.
                     *
                     * @return     Returns an Await Proxy.
                     */
                    await_proxy operator co_await() noexcept { return await_proxy{.this_ = this}; }

                private:

                    friend struct await_proxy;

                    descriptor_notification* self_;
                    descriptor*              desc_;
                    int                      flags_;
                    int                      fd_;
                    int32_t                  timeout_ = -1;
            };

            /**
             * @brief      Subscribe to events on a given file descriptor.
             *
             * @details    This function is not thread safe and can only be called once at a time.
             *             Multiple concurrent call will most likely fail, but is dependent on the
             *             epoll implementation.
             *
             * @param[in]  _fd   The file descriptor to subscribe to.
             *
             * @return     A descriptor_waiter on success, otherwise nullopt.
             */
            [[nodiscard]] std::optional<descriptor_waiter>
            subscribe(int _fd) noexcept;

            /**
             * @brief      Runs the internal service thread.
             */
            void
            run() noexcept;

            /**
             * @brief      Stops the internal service thread.
             */
            void
            stop() noexcept;

        private:

            /**
             * @brief      Notify a given descriptor with flags.
             *
             * @param      _awaiting  The awaiting descriptor.
             * @param[in]  _flags     The flags to set.
             */
            void
            notify(descriptor* _awaiting, int _flags) noexcept;

            /**
             * @brief      Runs the notification loop.
             *
             * @param[in]  _token  The token
             *
             */
            void
            notification_loop(std::stop_token _token) noexcept;

            void
            add_timer(timer_event* _timer) noexcept;

            void
            add_wakeup(descriptor* _timer) noexcept;

            void
            handle_timers() noexcept;

            void
            handle_event() noexcept;

            void
            add_deleted(descriptor* _desc) noexcept;

            void
            process_deleted() noexcept;

            void
            process_wakeups() noexcept;

            void
            add_in_action(descriptor* _desc) noexcept;

            void
            process_pending_actions();

            void
            transfer_timers(bool _always_update) noexcept;

            static constexpr uint64_t kNanoInSeconds = 1000000000;
            static constexpr uint64_t kDecriptorFlag = 0b1;
            static constexpr uint64_t kAddressMask   = ~kDecriptorFlag;

            std::jthread notification_loop_;

            std::atomic<descriptor*>  pending_in_action_;
            std::atomic<timer_event*> awaiting_timer_;
            std::atomic<descriptor*>  for_wake_up_;
            std::atomic<descriptor*>  for_deletion_;

            std::set<descriptor*> in_action_;
            timer_event* timers_;

            engine* engine_;

            int poll_descriptor_;
            int event_fd_;
            int timer_fd_;
    };

}   // namespace zab

#endif /* ZAB_DESCRIPTOR_NOTIFCATIONS_HPP_ */