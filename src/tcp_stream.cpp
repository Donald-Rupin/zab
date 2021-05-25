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

#include "zab/strong_types.hpp"

namespace zab {

    tcp_stream::internal_state::internal_state(
        engine*                                                     _engine,
        std::optional<descriptor_notification::descriptor_waiter>&& _desc)
        : mtx_(_engine), socket_(std::move(_desc)), last_error_(0)
    { }

    tcp_stream::tcp_stream(engine* _engine, int _fd)
        : state_(std::make_unique<internal_state>(
              _engine,
              _engine->get_notification_handler().subscribe(_fd)))
    {
        register_engine(*_engine);
        if (state_->socket_) { set_flags(); }
        else
        {

            state_->last_error_ = errno;
        }
    }

    tcp_stream::tcp_stream(
        engine*                                                     _engine,
        std::optional<descriptor_notification::descriptor_waiter>&& _awaiter)
        : state_(std::make_unique<internal_state>(_engine, std::move(_awaiter)))
    {
        register_engine(*_engine);
        if (state_->socket_) { set_flags(); }
        else
        {
            _engine = nullptr;
        }
    }

    tcp_stream::tcp_stream(tcp_stream&& _move) : state_(std::move(_move.state_))
    {
        register_engine(*_move.engine_);
    }

    void
    tcp_stream::set_flags() noexcept
    {
        state_->socket_->set_flags(
            descriptor_notification::kClosed | descriptor_notification::kException |
            descriptor_notification::kError | descriptor_notification::kRead);
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
        if (state_)
        {
            cancel_read();
            clean_up_and_forget(get_engine(), std::move(state_));
        }
    }

    simple_future<>
    tcp_stream::shutdown() noexcept
    {
        cancel_read();
        co_await clean_up(get_engine(), state_);
        state_ = nullptr;
    }

    simple_future<std::vector<char>>
    tcp_stream::read(size_t _amount, int64_t _timout) noexcept
    {
        thread_t rejoin_thread{engine_->get_event_loop().current_id()};
        state_->socket_->set_timeout(_timout);

        std::vector<char> buffer;
        buffer.reserve(_amount);

        while ((_amount && buffer.size() != _amount) || (!_amount && !buffer.size()))
        {
            auto to_ask = std::min(_amount - buffer.size(), kBufferSize);
            if (to_ask == 0) { to_ask = kBufferSize; }
            size_t old_size = buffer.size();
            buffer.resize(old_size + to_ask);

            if (int64_t numbytes =
                    ::recv(state_->socket_->file_descriptor(), buffer.data() + old_size, to_ask, 0);
                numbytes > 0)
            {
                buffer.resize(old_size + numbytes);
                co_await yield(now(), rejoin_thread);
            }
            else if (numbytes == 0)
            {
                buffer.resize(old_size);

                if (buffer.size()) { co_return buffer; }
                else
                {

                    co_return std::nullopt;
                }
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                buffer.resize(old_size);
                auto flags = co_await *state_->socket_;

                /* Timed out */
                if (!flags)
                {
                    if (buffer.size()) { co_return buffer; }
                    else
                    {
                        co_return std::nullopt;
                    }
                }
            }
            else
            {
                buffer.resize(old_size);
                state_->last_error_ = errno;
                if (buffer.size()) { co_return buffer; }
                else
                {
                    co_return std::nullopt;
                }
            }
        }

        co_return buffer;
    }

    guaranteed_future<size_t>
    tcp_stream::write(std::span<const char> _data) noexcept
    {
        thread_t rejoin_thread{engine_->get_event_loop().current_id()};

        /* Do not worry about out of order writes... */
        /* Mutex will resume in order of suspension  */
        auto   lock     = co_await state_->mtx_;
        size_t written  = 0;
        auto back_off = order::milli(1);
        while (written < _data.size())
        {
            auto to_send = std::min(_data.size() - written, kBufferSize);

            if (int64_t numbytes = ::send(
                    state_->socket_->file_descriptor(),
                    _data.data() + written,
                    to_send,
                    MSG_NOSIGNAL);
                numbytes > 0)
            {
                written += numbytes;
                co_await yield(now(), rejoin_thread);
            }
            else if (!numbytes || errno == EWOULDBLOCK || errno == EAGAIN)
            {
                co_await yield(now() + back_off, rejoin_thread);

                if (back_off < order::seconds(1)) { back_off.order_ <<= 1; }
                else
                {
                    state_->last_error_ = 0;
                    break;
                }
            }
            else
            {
                state_->last_error_ = errno;

                break;
            }
        }

        co_return written;
    }

    async_function<>
    tcp_stream::write_and_forget(std::vector<char> _data) noexcept
    {
        co_await write(_data);
    }

    simple_future<>
    tcp_stream::wait_for_writes() noexcept
    {
        /* When we resume, all writes initiated before us */
        /* Will be completed... */
        auto lock = co_await state_->mtx_;
    }

    async_function<>
    tcp_stream::clean_up_and_forget(
        engine*                           _engine,
        std::unique_ptr<internal_state>&& _state) noexcept
    {
        std::unique_ptr<internal_state> state(std::move(_state));
        co_await clean_up(_engine, state);
    }

    void
    tcp_stream::cancel_read() noexcept
    {
        state_->socket_->wake_up();
    }

    simple_future<>
    tcp_stream::clean_up(engine* _engine, std::unique_ptr<internal_state>& _state) noexcept
    {
        struct clean {

                ~clean()
                {
                    int       result;
                    socklen_t result_len = sizeof(result);
                    ::getsockopt(socket_, SOL_SOCKET, SO_ERROR, (char*) &result, &result_len);

                    if (::close(socket_) && errno != EBADF)
                    {
                        std::cerr << "zab: A tcp stream socket failed to close. "
                                  << "\n";
                    }
                }

                int socket_;

        } cln{_state->socket_->file_descriptor()};

        {
            /* wait for all writes to flush... */
            auto lock = co_await _state->mtx_;
        }

        thread_t rejoin_thread{_engine->get_event_loop().current_id()};

        if (::shutdown(cln.socket_, SHUT_WR) != -1)
        {
            /* now try and wait for the buffer to deplete... */
            for (int i = 0; i < 10; ++i)
            {
                int outstanding = 0;
                if (::ioctl(cln.socket_, TIOCOUTQ, &outstanding)) { break; }

                if (!outstanding) { break; }

                co_await zab::yield(_engine, {order::now()}, rejoin_thread);
            }

            char   buffer[256];
            size_t amount_flush = 0;
            /* now try and wait for client acknowledgement */
            for (int i = 0; i < 5;)
            {
                if (int64_t numbytes = ::recv(cln.socket_, buffer, sizeof(buffer), 0); !numbytes)
                {
                    break;
                }
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    ++i;
                    _state->socket_->set_timeout(100);

                    co_await *_state->socket_;
                }
                else
                {
                    amount_flush += numbytes;
                    /* If we flush more then 32 kb, then they are probably ignoring us */
                    if (amount_flush >= 1028 * 32) { break; }

                    co_await zab::yield(_engine, {order::now()}, rejoin_thread);
                }
            }
        }

        _state->socket_ = std::nullopt;
        /* now good to close! (RAII) */
    }

}   // namespace zab