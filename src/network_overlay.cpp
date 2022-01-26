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
 *  @file network_overlay.cpp
 *
 */

#include "zab/network_overlay.hpp"

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

#include "zab/event_loop.hpp"
#include "zab/first_of.hpp"
#include "zab/timer_service.hpp"

namespace zab {

    namespace {

        static simple_future<bool>
        do_close(engine* _engine, int _fd, event_loop::io_handle _ct)
        {
            struct cu {
                    ~cu()
                    {
                        if (fd_)
                        {
                            if (::close(fd_) < 0)
                            {
                                std::cerr << "async network failed to close the file descriptor "
                                          << fd_ << ". errno: " << errno << "\n";
                            }
                        }
                    }
                    int& fd_;
            } cleaner{_fd};

            if (_engine->current_id() == thread_t::any_thread())
            {
                co_await yield(_engine, thread_t::any_thread());
            }

            if (_ct) { co_await _engine->get_event_loop().cancel_event(_ct); }

            auto rc = co_await _engine->get_event_loop().close(_fd);
            _fd     = 0;

            if (rc && !*rc) { co_return true; }
            else
            {
                co_return false;
            }
        }

        static async_function<>
        background_close(engine* _engine, int _fd, event_loop::io_handle _ct)
        {
            co_await do_close(_engine, _fd, _ct);
        }

    }   // namespace

    tcp_acceptor::~tcp_acceptor()
    {
        if (acceptor_) { background_close(engine_, acceptor_, cancel_token_); }
    }

    tcp_connector::~tcp_connector()
    {
        if (connection_) { background_close(engine_, connection_, cancel_token_); }
    }

    tcp_acceptor::tcp_acceptor(engine* _engine)
        : engine_(_engine), cancel_token_(nullptr), acceptor_(0), last_error_(0)
    { }

    tcp_connector::tcp_connector(engine* _engine)
        : engine_(_engine), cancel_token_(nullptr), connection_(0), last_error_(0)
    { }

    tcp_acceptor::tcp_acceptor(tcp_acceptor&& _move) : tcp_acceptor() { swap(*this, _move); }

    tcp_acceptor&
    tcp_acceptor::operator=(tcp_acceptor&& _acceptor)
    {
        swap(*this, _acceptor);
        return *this;
    }

    void
    swap(tcp_acceptor& _first, tcp_acceptor& _second) noexcept
    {
        using std::swap;
        swap(_first.acceptor_, _second.acceptor_);
        swap(_first.last_error_, _second.last_error_);
        swap(_first.engine_, _second.engine_);
    }

    tcp_connector::tcp_connector(tcp_connector&& _move) : tcp_connector() { swap(*this, _move); }

    tcp_connector&
    tcp_connector::operator=(tcp_connector&& _connector)
    {
        swap(*this, _connector);
        return *this;
    }

    void
    swap(tcp_connector& _first, tcp_connector& _second) noexcept
    {
        using std::swap;
        swap(_first.connection_, _second.connection_);
        swap(_first.last_error_, _second.last_error_);
        swap(_first.engine_, _second.engine_);
    }

    simple_future<bool>
    tcp_acceptor::cancel()
    {
        if (cancel_token_)
        {
            auto tmp_ct   = cancel_token_;
            cancel_token_ = nullptr;
            auto rc       = co_await engine_->get_event_loop().cancel_event(tmp_ct);
            if (rc == event_loop::CancelResults::kDone) { co_return true; }
            else
            {
                co_return false;
            }
        }
    }

    simple_future<bool>
    tcp_acceptor::close()
    {
        auto tmp      = acceptor_;
        auto tmp_ct   = cancel_token_;
        acceptor_     = 0;
        cancel_token_ = nullptr;
        return do_close(engine_, tmp, tmp_ct);
    }

    simple_future<bool>
    tcp_connector::cancel()
    {

        if (cancel_token_)
        {
            auto tmp_ct   = cancel_token_;
            cancel_token_ = nullptr;
            auto rc       = co_await engine_->get_event_loop().cancel_event(tmp_ct);
            if (rc == event_loop::CancelResults::kDone) { co_return true; }
            else
            {
                co_return false;
            }
        }
    }

    simple_future<bool>
    tcp_connector::close()
    {
        auto tmp      = connection_;
        auto tmp_ct   = cancel_token_;
        connection_   = 0;
        cancel_token_ = nullptr;
        return do_close(engine_, tmp, tmp_ct);
    }

    bool
    tcp_acceptor::listen(int _family, std::uint16_t _port, int _backlog) noexcept
    {
        acceptor_ = ::socket(_family, SOCK_STREAM | SOCK_CLOEXEC, 0);

        if (!acceptor_ || acceptor_ < 0) [[unlikely]]
        {
            acceptor_   = 0;
            last_error_ = errno;
            return false;
        }

        unsigned long on = 1;
        if (::setsockopt(acceptor_, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on)) != 0)
            [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        _port = ::htons(_port);

        struct sockaddr_storage add;
        ::memset(&add, 0, sizeof(add));
        if (_family == AF_INET)
        {
            struct sockaddr_in* in4 = (struct sockaddr_in*) &add;
            in4->sin_family         = AF_INET;
            in4->sin_port           = _port;
            in4->sin_addr.s_addr    = INADDR_ANY;
        }
        else if (_family == AF_INET6)
        {
            struct sockaddr_in6* in6 = (struct sockaddr_in6*) &add;
            in6->sin6_family         = AF_INET6;
            in6->sin6_port           = _port;
            in6->sin6_addr           = in6addr_any;
        }
        else
        {

            last_error_ = EINVAL;
            return false;
        }

        if (::bind(acceptor_, (struct sockaddr*) &add, sizeof(add)) != 0) [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        if (::listen(acceptor_, _backlog) != 0) [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        return true;
    }

    simple_future<tcp_stream>
    tcp_acceptor::accept() noexcept
    {
        std::optional<tcp_stream> stream;

        struct sockaddr_storage address;
        socklen_t               addlen = sizeof(address);
        ::memset(&address, 0, sizeof(address));

        auto rc = co_await engine_->get_event_loop().accept(
            acceptor_,
            (struct sockaddr*) &address,
            &addlen,
            SOCK_CLOEXEC,
            &cancel_token_);

        cancel_token_ = nullptr;

        if (rc && *rc >= 0) { stream.emplace(engine_, *rc); }
        else if (rc)
        {
            last_error_ = -*rc;
        }

        co_return stream;
    }

    reusable_future<tcp_stream>
    tcp_acceptor::get_accepter() noexcept
    {
        while (true)
        {
            std::optional<tcp_stream> stream;

            struct sockaddr_storage address;
            socklen_t               addlen = sizeof(address);
            ::memset(&address, 0, sizeof(address));

            auto rc = (co_await pause(
                           [&](auto* _pp) noexcept
                           {
                               cancel_token_ = _pp;
                               engine_->get_event_loop().accept(
                                   _pp,
                                   acceptor_,
                                   (struct sockaddr*) &address,
                                   &addlen,
                                   SOCK_CLOEXEC);
                           }))
                          .data_;

            cancel_token_ = nullptr;

            if (rc >= 0) { stream.emplace(engine_, rc); }

            else if (rc)
            {
                last_error_ = -rc;
            }

            co_yield stream;
        }

        co_return std::nullopt;
    }

    simple_future<tcp_stream>
    tcp_connector::connect(struct sockaddr_storage* _details, socklen_t _size) noexcept
    {

        std::optional<tcp_stream> stream;
        if (!connection_)
        {
            if (connection_ = ::socket(_details->ss_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
                connection_ < 0) [[unlikely]]
            {
                last_error_ = errno;
                co_return std::nullopt;
            }
        }

        auto rc = co_await engine_->get_event_loop()
                      .connect(connection_, (sockaddr*) _details, _size, &cancel_token_);

        cancel_token_ = nullptr;

        if (rc && !*rc)
        {
            stream.emplace(engine_, connection_);
            connection_ = 0;
        }
        else if (rc)
        {
            last_error_ = -*rc;
        }

        co_return stream;
    }

}   // namespace zab
