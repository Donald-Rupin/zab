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
 *  @file signal_handler.cpp
 *
 */

#include "zab/signal_handler.hpp"

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <unistd.h>

#include "zab/engine.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    namespace {
        std::atomic<signal_handler*> handler_ = {nullptr};
    }

    signal_handler::signal_handler(engine* _engine)
        : handlers_mtx_(std::make_unique<std::mutex>()), engine_(_engine)
    {

        signal_handler* test = nullptr;
        if (!handler_.is_lock_free() || !handler_.compare_exchange_strong(test, this)) { return; }

        fds_[0] = 0;
        fds_[1] = 0;

        if (int error = ::pipe2(fds_, O_NONBLOCK); error == -1)
        {
            std::cerr << "signal_handler ->  signal_handler -> Failed to create pipe. errno:"
                      << errno;
            abort();
        }
    }

    signal_handler::~signal_handler()
    {
        signal_handler* test = this;
        handler_.compare_exchange_strong(test, nullptr);

        if (close(fds_[0]) || close(fds_[1]))
        {

            std::cerr
                << "signal_handler -> Failed to close Notifier pipe during deconstruction. errno:"
                << errno;
            abort();
        }
    }

    async_function<>
    signal_handler::run()
    {
        if (handler_.load() != this) { co_return; }

        auto& not_handler = engine_->get_notification_handler();

        auto descriptor = not_handler.subscribe(fds_[0]);

        if (!descriptor) [[unlikely]]
        {
            std::cerr << "signal_handler ->  Failed to get descriptor at run time \n";
            abort();
        }

        std::unique_ptr<descriptor_notification::descriptor_op> read_op;
        running_ = true;
        while (running_)
        {
            int rc;
            if (read_op) { rc = co_await *read_op; }
            else
            {
                read_op = co_await descriptor->start_read_operation();

                if (!read_op)
                {
                    std::cerr << "signal_handler ->  Failed to start read_operation \n";
                    abort();
                }

                rc = read_op->flags();
            }

            if (rc | descriptor_notification::kRead)
            {
                char buffer;
                if (int rc = ::read(fds_[0], &buffer, 1);
                    rc < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    std::cerr << "signal_handler -> read failed. errno:" << errno << "\n";
                    co_return;
                }

                int signal = static_cast<int>(buffer);

                std::scoped_lock lck(*handlers_mtx_);
                if (auto it = handlers_.find(signal); it != handlers_.end())
                {
                    for (const auto& [thread, functor] : it->second)
                    {
                        engine_->execute(
                            [functor = functor, signal]() noexcept { functor(signal); },
                            order_t{}, /* Signal handlers go first */
                            thread);
                    }
                }
            }
            else
            {
                running_ = false;
            }
        }
    }

    bool
    signal_handler::is_active()
    {
        return handler_.load() == this;
    }

    void
    signal_handler::stop()
    {
        if (handler_.load() != this || !handler_.is_lock_free()) { return; }

        running_ = false;

        for (const auto& [sig, handlers] : handlers_)
        {
            ::signal(sig, SIG_DFL);
        }

        char sig = SIGKILL;
        if (int error = ::write(fds_[1], &sig, 1); error < 0)
        {
            std::cerr << "signal_handler -> Failed to write to pipe on stop. errno:" << errno
                      << "\n";
        }
    }

    bool
    signal_handler::handle(int _sig, thread_t _thread, handler&& _function) noexcept
    {
        if (handler_.load() != this || !handler_.is_lock_free()) { return false; }

        std::scoped_lock lck(*handlers_mtx_);
        handlers_[_sig].emplace_back(_thread, std::move(_function));

        /** Its new... */
        if (handlers_[_sig].size() == 1)
        {

            auto handler = +[](int _signal)
            {
                signal_handler* self = handler_.load();

                char sig = _signal;
                if (int error = ::write(self->fds_[1], &sig, 1); error < 0)
                {

                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        std::cerr << "signal_handler -> Failed to write to "
                                     "pipe. signal: "
                                  << _signal << ", errno: " << errno << "\n";
                    }
                }
            };

            return ::signal(_sig, handler) == 0 ? true : false;
        }

        return true;
    }

}   // namespace zab