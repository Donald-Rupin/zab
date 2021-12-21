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
 *  @file tcp_stream.cpp
 *
 */

#include "zab/tcp_stream.hpp"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <atomic>
#include <coroutine>
#include <errno.h>
#include <ifaddrs.h>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <optional>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "zab/first_of.hpp"
#include "zab/for_each.hpp"
#include "zab/strong_types.hpp"
#include "zab/timer_service.hpp"

namespace zab {

    tcp_stream::internal_state::internal_state(
        std::optional<descriptor_notification::notifier>&& _desc)
        : socket_(std::move(_desc)), last_error_(0)
    { }

    tcp_stream::internal_state::~internal_state() { }

    tcp_stream::tcp_stream(engine* _engine, int _fd)
        : state_(
              std::make_unique<internal_state>(_engine->get_notification_handler().subscribe(_fd)))
    {
        register_engine(*_engine);
        if (!state_->socket_) { state_->last_error_ = errno; }
    }

    tcp_stream::tcp_stream(
        engine*                                            _engine,
        std::optional<descriptor_notification::notifier>&& _awaiter)
        : state_(std::make_unique<internal_state>(std::move(_awaiter)))
    {
        register_engine(*_engine);
        if (!state_->socket_) { _engine = nullptr; }
    }

    tcp_stream::tcp_stream(tcp_stream&& _move) : state_(std::move(_move.state_))
    {
        register_engine(*_move.engine_);
    }

    tcp_stream&
    tcp_stream::operator=(tcp_stream&& _move)
    {
        if (state_) { clean_up_and_forget(get_engine(), std::move(state_)); }
        state_ = std::move(_move.state_);
        register_engine(*_move.engine_);

        return *this;
    }

    tcp_stream::~tcp_stream()
    {
        if (state_) { clean_up_and_forget(get_engine(), std::move(state_)); }
    }

    simple_future<>
    tcp_stream::shutdown() noexcept
    {
        co_await clean_up(get_engine(), std::move(state_));
    }

    simple_future<std::vector<char>>
    tcp_stream::read(size_t _amount, descriptor_notification::descriptor_op* _op) noexcept
    {
        std::vector<char> buffer(_amount);

        auto amount = co_await read(buffer, _op);

        buffer.resize(amount);

        co_return buffer;
    }

    [[nodiscard]] guaranteed_future<std::size_t>
    tcp_stream::read(std::span<char> _data, descriptor_notification::descriptor_op* _op) noexcept
    {
        if (_op && _op->op_type() == descriptor_notification::descriptor_op::type::kWrite)
            [[unlikely]]
        {
            state_->last_error_ = 0;
            co_return 0;
        }

        bool     rejoin      = _op == nullptr;
        bool     left_thread = false;
        thread_t rejoin_thread{engine_->get_event_loop().current_id()};

        std::size_t so_far = 0;
        const auto  sd     = state_->socket_->file_descriptor();

        std::unique_ptr<descriptor_notification::descriptor_op> op;
        do
        {
            if (int64_t numbytes = ::recv(sd, _data.data() + so_far, _data.size() - so_far, 0);
                numbytes > 0)
            {
                so_far += numbytes;
            }
            else if (numbytes == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::uint32_t flags = 0;

                if (_op) { flags = co_await *_op; }
                else
                {
                    left_thread = true;
                    op          = co_await start_read_operation();

                    if (!op)
                    {
                        state_->last_error_ = 0;
                        break;
                    }
                    _op   = op.get();
                    flags = op->flags();
                }

                /* No flags means something happened internally */
                /* Like a cancel or deconstruction              */
                if (!flags) { break; }
            }
            else
            {
                state_->last_error_ = errno;
                break;
            }

        } while (so_far != _data.size());

        if (rejoin && left_thread) { co_await yield(rejoin_thread); }

        co_return so_far;
    }

    simple_future<std::vector<char>>
    tcp_stream::read_some(size_t _max, descriptor_notification::descriptor_op* _op) noexcept
    {
        std::vector<char> data(_max);

        if (auto size = co_await read_some(data, _op); size)
        {
            data.resize(size);
            co_return data;
        }
        else
        {
            co_return std::nullopt;
        }
    }

    guaranteed_future<std::size_t>
    tcp_stream::read_some(
        std::span<char>                         _data,
        descriptor_notification::descriptor_op* _op) noexcept
    {
        if (_op && _op->op_type() == descriptor_notification::descriptor_op::type::kWrite)
            [[unlikely]]
        {
            state_->last_error_ = 0;
            co_return 0;
        }

        bool     rejoin      = _op == nullptr;
        bool     left_thread = false;
        thread_t rejoin_thread{engine_->get_event_loop().current_id()};

        std::size_t so_far = 0;
        const auto  sd     = state_->socket_->file_descriptor();

        std::unique_ptr<descriptor_notification::descriptor_op> op;

        if (int64_t numbytes = ::recv(sd, _data.data(), _data.size(), 0); numbytes > 0)
        {
            so_far += numbytes;
        }
        else if (numbytes == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        {
            std::uint32_t flags = 0;

            if (_op) { flags = co_await *_op; }
            else
            {
                left_thread = true;
                op          = co_await start_read_operation();

                if (!op) { state_->last_error_ = 0; }
                else
                {
                    _op = op.get();

                    flags = op->flags();
                }
            }

            /* No flags means something happened internally */
            /* Like a cancel or deconstruction              */
            if (flags)
            {
                if (int64_t numbytes = ::recv(sd, _data.data(), _data.size(), 0); numbytes >= 0)
                {
                    so_far += numbytes;
                }
                else
                {
                    state_->last_error_ = errno;
                }
            }
        }
        else
        {
            state_->last_error_ = errno;
        }

        if (rejoin && left_thread) { co_await yield(rejoin_thread); }

        co_return so_far;
    }

    guaranteed_future<size_t>
    tcp_stream::write(
        std::span<const char>                   _data,
        descriptor_notification::descriptor_op* _op) noexcept
    {
        if (_op && _op->op_type() == descriptor_notification::descriptor_op::type::kRead)
            [[unlikely]]
        {
            state_->last_error_ = 0;
            co_return 0;
        }

        bool     rejoin      = _op == nullptr;
        bool     left_thread = false;
        thread_t rejoin_thread{engine_->get_event_loop().current_id()};

        const auto sd      = state_->socket_->file_descriptor();
        size_t     written = 0;

        std::unique_ptr<descriptor_notification::descriptor_op> op;
        do
        {

            if (int64_t numbytes =
                    ::send(sd, _data.data() + written, _data.size() - written, MSG_NOSIGNAL);
                numbytes > 0)
            {
                written += numbytes;
            }
            else if (!numbytes || errno == EWOULDBLOCK || errno == EAGAIN)
            {
                std::uint32_t flags = 0;
                if (_op) { flags = co_await *_op; }
                else
                {
                    left_thread = true;
                    op          = co_await start_write_operation();

                    if (!op)
                    {
                        state_->last_error_ = 0;
                        break;
                    }

                    flags = op->flags();
                    _op   = op.get();
                }

                /* No flags means something happened internally */
                /* Like a cancel or deconstruction              */
                if (!flags) { break; }
            }
            else
            {
                state_->last_error_ = errno;
            }

        } while (written < _data.size());

        if (rejoin && left_thread) { co_await yield(rejoin_thread); }

        co_return written;
    }

    guaranteed_future<std::unique_ptr<descriptor_notification::descriptor_op>>
    tcp_stream::start_write_operation() noexcept
    {
        return state_->socket_->start_write_operation();
    }

    [[nodiscard]] guaranteed_future<std::unique_ptr<descriptor_notification::descriptor_op>>
    tcp_stream::start_read_operation() noexcept
    {
        return state_->socket_->start_read_operation();
    }

    async_function<>
    tcp_stream::clean_up_and_forget(
        engine*                           _engine,
        std::unique_ptr<internal_state>&& _state) noexcept
    {
        if (_state) { co_await clean_up(_engine, std::move(_state)); }
    }

    void
    tcp_stream::cancel() noexcept
    { }

    simple_future<>
    tcp_stream::clean_up(engine* _engine, std::unique_ptr<internal_state>&& _state) noexcept
    {
        /* RAII is important here in ordering */
        struct clean {

                ~clean()
                {
                    int       result;
                    socklen_t result_len = sizeof(result);
                    ::getsockopt(socket_, SOL_SOCKET, SO_ERROR, (char*) &result, &result_len);

                    if (::close(socket_) && errno != EBADF)
                    {
                        std::cerr << "zab: A tcp stream socket failed to close.\n";
                    }
                }

                int socket_;

        } cln{_state->socket_->file_descriptor()};

        std::unique_ptr<internal_state> state(std::move(_state));

        state->socket_->cancel();

        thread_t rejoin_thread{_engine->get_event_loop().current_id()};
        if (::shutdown(cln.socket_, SHUT_WR) != -1)
        {
            /* now try and wait for the buffer to deplete... */
            for (int i = 0; i < 10; ++i)
            {
                int outstanding = 0;
                if (::ioctl(cln.socket_, TIOCOUTQ, &outstanding)) { break; }

                if (!outstanding) { break; }

                co_await zab::yield(_engine, order::milli(10), rejoin_thread);
            }

            std::vector<char> buffer(1028);
            size_t            amount_flush = 0;

            std::unique_ptr<descriptor_notification::descriptor_op> op;
            /* now try and wait for client acknowledgement */
            for (int i = 0; i < 5;)
            {
                if (int64_t numbytes = ::recv(cln.socket_, buffer.data(), buffer.size(), 0);
                    !numbytes)
                {
                    break;
                }
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    if (!op) { op = co_await state->socket_->start_read_operation(); }
                    else
                    {
                        co_await *op;
                    }
                    ++i;
                }
                else
                {
                    amount_flush += numbytes;
                    /* If we flush more then 32 kb, then they are probably ignoring us */
                    if (amount_flush >= 1028 * 32) { break; }
                }
            }
        }

        state->socket_ = std::nullopt;
        /* now good to close! (RAII) */
    }

}   // namespace zab