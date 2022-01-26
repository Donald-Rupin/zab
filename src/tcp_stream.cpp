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
#include "zab/yield.hpp"

namespace zab {

    namespace {

        simple_future<>
        clean_up(engine* _engine, int _fd, event_loop::io_handle _ct) noexcept
        {
            /* RAII is important here in ordering */
            struct clean {

                    ~clean()
                    {
                        if (socket_)
                        {

                            if (::close(socket_))
                            {
                                std::cerr << "zab: A tcp stream socket failed to close.\n";
                            }
                        }
                    }

                    int& socket_;

            } cln{_fd};

            if (_engine->current_id() == thread_t::any_thread())
            {
                co_await yield(_engine, thread_t::any_thread());
            }

            auto thread = _engine->current_id();

            /* Shut down pending ops... */
            if (_ct) { co_await _engine->get_event_loop(thread).cancel_event(_ct); }

            if (::shutdown(cln.socket_, SHUT_WR) != -1)
            {
                /* now try and wait for the buffer to deplete... */
                for (int i = 0; i < 10; ++i)
                {
                    int outstanding = 0;
                    if (::ioctl(cln.socket_, TIOCOUTQ, &outstanding)) { break; }

                    if (!outstanding) { break; }

                    co_await zab::yield(_engine, order::milli(10), thread);
                }
            }

            /* Clear any errors */
            int       result;
            socklen_t result_len = sizeof(result);
            ::getsockopt(_fd, SOL_SOCKET, SO_ERROR, (char*) &result, &result_len);

            auto pp = co_await pause([&](auto* pp) noexcept
                                     { _engine->get_event_loop().close(pp, _fd); });

            if (pp.data_ == 0) { _fd = 0; }
        }

        static async_function<>
        background_clean_up(engine* _engine, int _fd, event_loop::io_handle _ct)
        {
            co_await clean_up(_engine, _fd, _ct);
        }

    }   // namespace

    tcp_stream::tcp_stream(engine* _engine, int _fd)
        : engine_(_engine), cancel_token_(nullptr), connection_(_fd), last_error_(0)
    { }

    tcp_stream::tcp_stream(tcp_stream&& _move) : tcp_stream() { swap(*this, _move); }

    tcp_stream::~tcp_stream()
    {
        if (connection_) { background_clean_up(engine_, connection_, cancel_token_); }
    }

    void
    swap(tcp_stream& _first, tcp_stream& _second) noexcept
    {
        using std::swap;
        swap(_first.engine_, _second.engine_);
        swap(_first.cancel_token_, _second.cancel_token_);
        swap(_first.connection_, _second.connection_);
        swap(_first.last_error_, _second.last_error_);
    }

    simple_future<>
    tcp_stream::shutdown() noexcept
    {
        auto tmp      = connection_;
        auto tmp_ct   = cancel_token_;
        connection_   = 0;
        cancel_token_ = nullptr;
        return clean_up(engine_, tmp, tmp_ct);
    }

    simple_future<std::vector<char>>
    tcp_stream::read(size_t _amount) noexcept
    {
        std::vector<char> buffer(_amount);

        auto amount = co_await read(buffer);

        buffer.resize(amount);

        co_return buffer;
    }

    [[nodiscard]] guaranteed_future<std::size_t>
    tcp_stream::read(std::span<char> _data) noexcept
    {
        std::size_t so_far = 0;
        while (so_far != _data.size())
        {
            auto size =
                co_await read_some(std::span<char>(_data.data() + so_far, _data.size() - so_far));

            if (size) { so_far += size; }
            else
            {
                break;
            }
        }

        co_return so_far;
    }

    simple_future<std::vector<char>>
    tcp_stream::read_some(size_t _max) noexcept
    {
        std::vector<char> data(_max);
        if (auto size = co_await read_some(data); size)
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
    tcp_stream::read_some(std::span<char> _data) noexcept
    {
        auto rc = co_await engine_->get_event_loop().recv(
            connection_,
            std::span<std::byte>((std::byte*) _data.data(), _data.size()),
            0,
            &cancel_token_);
        cancel_token_ = nullptr;

        if (rc && *rc > 0) { co_return *rc; }
        else
        {
            co_return 0;
        }
    }

    reusable_future<std::size_t>
    tcp_stream::get_reader(op_control* _oc) noexcept
    {
        int rc;
        do
        {
            auto pp = (co_await pause(
                [&](auto* _pp) noexcept
                {
                    cancel_token_ = _pp;
                    engine_->get_event_loop().recv(
                        _pp,
                        connection_,
                        std::span<std::byte>((std::byte*) _oc->data_, _oc->size_),
                        0);
                }));

            rc            = pp.data_;
            cancel_token_ = nullptr;

            if (rc > 0) { co_yield rc; }
            else
            {
                rc = 0;
            }

        } while (rc);

        co_return 0;
    }

    [[nodiscard]] guaranteed_future<std::size_t>
    tcp_stream::read_fixed(std::span<std::byte> _data, int _index) noexcept
    {

        auto rc = co_await engine_->get_event_loop()
                      .fixed_read(connection_, _data, 0, _index, &cancel_token_);
        cancel_token_ = nullptr;

        if (rc && *rc > 0) { co_return *rc; }
        else
        {
            co_return 0;
        }
    }

    guaranteed_future<size_t>
    tcp_stream::write(std::span<const char> _data) noexcept
    {
        std::size_t total = 0;
        while (total != _data.size())
        {
            auto amount_to_write = std::min(_data.size() - total, 65536ul);

            auto rc = co_await engine_->get_event_loop().send(
                connection_,
                std::span<const std::byte>(
                    (const std::byte*) _data.data() + total,
                    amount_to_write),
                MSG_NOSIGNAL,
                &cancel_token_);
            cancel_token_ = nullptr;

            if (rc && *rc > 0) { total += *rc; }
            else
            {
                break;
            }
        }

        co_return total;
    }

    reusable_future<std::size_t>
    tcp_stream::get_writer(op_control* _oc) noexcept
    {
        int         rc;
        std::size_t total = 0;
        do
        {
            while (total != _oc->size_)
            {
                auto amount_to_write = std::min(_oc->size_ - total, 65536ul);

                auto pp = (co_await pause(
                    [&](auto* _pp) noexcept
                    {
                        cancel_token_ = _pp;
                        engine_->get_event_loop().send(
                            _pp,
                            connection_,
                            std::span<const std::byte>(
                                (const std::byte*) _oc->data_ + total,
                                amount_to_write),
                            MSG_NOSIGNAL);
                    }));

                rc            = pp.data_;
                cancel_token_ = nullptr;

                if (rc > 0) { total += rc; }
                else
                {
                    rc = 0;
                    break;
                }
            }

            cancel_token_ = nullptr;

            if (total) { co_yield total; }

            total = 0;

        } while (rc);

        co_return total;
    }

    [[nodiscard]] guaranteed_future<std::size_t>
    tcp_stream::write_fixed(std::span<const std::byte> _data, int _index) noexcept
    {
        std::size_t total = 0;
        while (total != _data.size())
        {
            auto amount_to_write = std::min(_data.size() - total, 65536ul);

            auto rc = co_await engine_->get_event_loop().fixed_write(
                connection_,
                std::span<const std::byte>(
                    (const std::byte*) _data.data() + total,
                    amount_to_write),
                0,
                _index,
                &cancel_token_);
            cancel_token_ = nullptr;

            if (rc && *rc > 0) { total += *rc; }
            else
            {
                break;
            }
        }

        co_return total;
    }

    simple_future<bool>
    tcp_stream::cancel() noexcept
    {
        if (cancel_token_)
        {
            auto rc       = co_await engine_->get_event_loop().cancel_event(cancel_token_);
            cancel_token_ = nullptr;

            co_return rc == event_loop::CancelResults::kDone;
        }

        co_return true;
    }

}   // namespace zab