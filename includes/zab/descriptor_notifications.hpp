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
#include <atomic>
#include <cassert>
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
#include "zab/simple_future.hpp"
#include "zab/spin_lock.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class engine;

    /**
     * @brief      This class implements an asynchronous `epoll` based descriptor notification
     * service.
     */
    class descriptor_notification {

        public:

            class descriptor;

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
             * @brief     Cleans up the resources.
             */
            void
            purge();

            /**
             * @brief      Convince types for the epoll macro equivalent.
             */
            enum NoticationType {
                kError     = EPOLLERR,
                kRead      = EPOLLIN | EPOLLRDNORM,
                kWrite     = EPOLLOUT | EPOLLWRNORM,
                kException = EPOLLPRI,
                kClosed    = EPOLLRDHUP | EPOLLHUP
            };

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

            class descriptor_op {

                    friend class descriptor_notification;
                    friend struct awaiter;

                public:

                    enum class type {
                        kWrite,
                        kRead,
                        kReadWrite
                    };

                    descriptor_op(descriptor* _desc, type _type, std::coroutine_handle<> _handle);

                    descriptor_op(const descriptor_op&) = delete;

                    ~descriptor_op();

                    struct awaiter {

                            bool
                            await_suspend(std::coroutine_handle<> _awaiter) noexcept
                            {
                                assert(result_);
                                assert(!result_->handle_);

                                result_->handle_ = _awaiter;
                                result_->flags_  = 0;

                                if (result_->desc_->re_arm(result_->type_)) { return true; }
                                else
                                {
                                    result_->handle_ = nullptr;
                                    return false;
                                }
                            }

                            bool
                            await_ready() const noexcept
                            {
                                return !result_->desc_;
                            }

                            int
                            await_resume() const noexcept
                            {
                                return result_->flags_;
                            }

                            descriptor_op* result_;
                    };

                    awaiter operator co_await() noexcept { return awaiter{this}; }

                    int
                    flags() const noexcept
                    {
                        return flags_;
                    }

                    type
                    op_type() const noexcept
                    {
                        return type_;
                    }

                private:

                    descriptor*    desc_;
                    descriptor_op* next_;

                    std::coroutine_handle<> handle_;
                    int                     flags_;

                    type type_;
            };

            class notifier {

                    friend class descriptor_notification;

                public:

                    ~notifier();

                    notifier(notifier&& _move);

                    notifier&
                    operator=(notifier&&);

                    notifier(const notifier&) = delete;

                    [[nodiscard]] inline guaranteed_future<std::unique_ptr<descriptor_op>>
                    start_write_operation() noexcept
                    {
                        return start_operation(descriptor_op::type::kWrite);
                    }

                    [[nodiscard]] inline guaranteed_future<std::unique_ptr<descriptor_op>>
                    start_read_operation() noexcept
                    {
                        return start_operation(descriptor_op::type::kRead);
                    }

                    [[nodiscard]] inline guaranteed_future<std::unique_ptr<descriptor_op>>
                    start_read_write_operation() noexcept
                    {
                        return start_operation(descriptor_op::type::kReadWrite);
                    }

                    [[nodiscard]] guaranteed_future<std::unique_ptr<descriptor_op>>
                    start_operation(descriptor_op::type _type) noexcept;

                    int
                    file_descriptor() const noexcept
                    {
                        return desc_->file_descriptor();
                    }

                    inline void
                    cancel()
                    {
                        if (desc_) { desc_->cancel(); }
                    }

                private:

                    notifier(descriptor* _desc, descriptor_notification* _notif);

                    void
                    remove();

                    descriptor*              desc_;
                    descriptor_notification* notif_;
            };

            [[nodiscard]] std::optional<notifier>
            subscribe(int _fd) noexcept;

            void
            remove(notifier& _notifier) noexcept;

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
                    descriptor(int _fd, int _poll);

                    /**
                     * @brief      Destroys the object. This is a non-owning object.
                     */
                    ~descriptor();

                    /**
                     * @brief      Cannot be copied.
                     *
                     * @param[in]  <unnamed>
                     */
                    descriptor(const descriptor&) = delete;

                    void
                    add_operation(
                        descriptor_op::type             _type,
                        std::coroutine_handle<>         _handle,
                        std::unique_ptr<descriptor_op>& _op) noexcept;

                    bool
                    re_arm(descriptor_op::type _type) noexcept;

                    bool
                    set_epol() noexcept;

                    void
                    remove(descriptor_op* _to_remove);

                    int
                    file_descriptor() const noexcept
                    {
                        return fd_;
                    }

                    void
                    cancel();

                private:

                    bool
                    re_arm_internal(descriptor_op::type _type) noexcept;

                    std::mutex     mtx_;
                    descriptor_op* ops_head_;
                    descriptor_op* ops_tail_;
                    int            fd_;
                    int            poll_descriptor_;
                    int            flags_;
            };

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
            handle_event() noexcept;

            void
            destroy(descriptor* _to_delete) const noexcept;

            std::jthread notification_loop_;

            struct safe_queue {

                    spin_lock mtx_;

                    std::deque<descriptor*> decs_;
            };

            safe_queue for_deletion_;

            safe_queue pending_action_;

            std::set<descriptor*> in_action_;

            engine* engine_;

            int poll_descriptor_;
            int event_fd_;
    };

}   // namespace zab

#endif /* ZAB_DESCRIPTOR_NOTIFCATIONS_HPP_ */