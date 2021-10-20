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

#include "zab/first_of.hpp"
#include "zab/timer_service.hpp"

namespace zab {

    namespace {
        inline void
        CloseSocket(int _socket)
        {
            /* Clean any errors on the sockets... */
            int       result;
            socklen_t result_len = sizeof(result);
            ::getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char*) &result, &result_len);

            if (::close(_socket) && errno != EBADF)
            {

                std::cerr << "zab: A network overlay socket failed to close. "
                          << "\n";
            }
        }
    }   // namespace

    tcp_acceptor::~tcp_acceptor()
    {
        if (waiter_)
        {
            auto desc = waiter_->file_descriptor();
            waiter_   = std::nullopt;
            CloseSocket(desc);
        }
    }

    tcp_connector::~tcp_connector()
    {
        if (waiter_)
        {
            auto desc = waiter_->file_descriptor();
            waiter_   = std::nullopt;
            CloseSocket(desc);
        }
    }

    tcp_acceptor::tcp_acceptor(engine* _engine) : last_error_(0) { register_engine(*_engine); }

    tcp_connector::tcp_connector(engine* _engine) : last_error_(0) { register_engine(*_engine); }

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
        swap(_first.waiter_, _second.waiter_);
        swap(_first.last_error_, _second.last_error_);
        using Acc = engine_enabled<tcp_acceptor>;
        swap(*((Acc*) &_first), *((Acc*) &_second));
    }

    tcp_connector::tcp_connector(tcp_connector&& _move) : tcp_connector() { swap(*this, _move); }

    tcp_connector&
    tcp_connector::operator=(tcp_connector&& _acceptor)
    {
        swap(*this, _acceptor);
        return *this;
    }

    void
    swap(tcp_connector& _first, tcp_connector& _second) noexcept
    {
        using std::swap;
        swap(_first.waiter_, _second.waiter_);
        swap(_first.last_error_, _second.last_error_);
        using Con = engine_enabled<tcp_connector>;
        swap(*((Con*) &_first), *((Con*) &_second));
    }

    bool
    tcp_acceptor::listen(int _family, std::uint16_t _port, int _backlog) noexcept
    {
        auto test = ::socket(_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

        if (!test || test < 0) [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        unsigned long on = 1;
        if (::setsockopt(test, SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on)) != 0) [[unlikely]]
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

        if (::bind(test, (struct sockaddr*) &add, sizeof(add)) != 0) [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        if (::listen(test, _backlog) != 0) [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        waiter_ = get_engine()->get_notification_handler().subscribe(test);

        if (!waiter_) [[unlikely]]
        {
            last_error_ = errno;
            return false;
        }

        waiter_->set_flags(
            descriptor_notification::kError | descriptor_notification::kException |
            descriptor_notification::kRead);

        return true;
    }

    simple_future<tcp_stream>
    tcp_acceptor::accept(int _timeout) noexcept
    {
        struct sockaddr_storage address;
        socklen_t               addlen = sizeof(address);
        ::memset(&address, 0, sizeof(address));
        while (true)
        {
            if (int sd = ::accept4(
                    waiter_->file_descriptor(),
                    (struct sockaddr*) &address,
                    &addlen,
                    SOCK_NONBLOCK | SOCK_CLOEXEC);
                sd >= 0)
            {
                tcp_stream stream(get_engine(), sd);

                if (!stream.last_error()) { co_return stream; }
                else
                {

                    last_error_ = stream.last_error();
                    co_return std::nullopt;
                }
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* We do not care if its an error */
                /* Next call to accept will reveal! */
                std::uint32_t flags = 0;

                if (_timeout < 1) { flags = co_await *waiter_; }
                else
                {
                    auto result = co_await first_of(
                        engine_,
                        AwaitWrapper(*waiter_),
                        engine_->get_timer().wait_proxy(_timeout));

                    auto index = result.index();

                    if (index == 0) { flags = std::get<int>(result); }

                    if (!flags)
                    {
                        /* must of timed out...*/
                        last_error_ = 0;
                        co_return std::nullopt;
                    }
                }
            }
            else
            {

                last_error_ = errno;
                co_return std::nullopt;
            }
        }
    }

    simple_future<tcp_stream>
    tcp_connector::connect(
        struct sockaddr_storage* _details,
        socklen_t                _size,
        int                      _timeout) noexcept
    {
        if (!waiter_)
        {
            int socket;
            if (socket =
                    ::socket(_details->ss_family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
                socket < 0) [[unlikely]]
            {
                last_error_ = errno;
                co_return std::nullopt;
            }

            waiter_ = get_engine()->get_notification_handler().subscribe(socket);

            if (!waiter_) [[unlikely]]
            {
                last_error_ = errno;
                co_return std::nullopt;
            }
        }

        if (::connect(waiter_->file_descriptor(), (const sockaddr*) _details, _size) == 0 ||
            errno == EISCONN)
        {
            co_return tcp_stream(get_engine(), std::move(waiter_));
        }
        else if (
            errno == EINPROGRESS || errno == EAGAIN || errno == EWOULDBLOCK || errno == EALREADY)
        {
            /* We have EALREADY in case they call connect again after a time out... */
            /* In that case, we wait for writabilty or an error. */
            // waiter_->set_timeout(_timeout);

            waiter_->set_flags(
                descriptor_notification::kClosed | descriptor_notification::kException |
                descriptor_notification::kError | descriptor_notification::kWrite);

            /* We do not care if its an error */
            /* Next call to accept will reveal! */
            std::uint32_t flags = 0;

            if (_timeout < 1) { flags = co_await *waiter_; }
            else
            {
                auto result = co_await first_of(
                    engine_,
                    AwaitWrapper(*waiter_),
                    engine_->get_timer().wait_proxy(_timeout));

                auto index = result.index();

                if (index == 0) { flags = std::get<int>(result); }

                if (!flags)
                {
                    /* must of timed out...*/
                    last_error_ = 0;
                    co_return std::nullopt;
                }
            }

            if (flags & descriptor_notification::kWrite)
            {
                /* I think its odd that a failed connect socket can be writable... */
                /* but we will handle this case anyways...                         */
                struct sockaddr addr;
                socklen_t       addrlen = sizeof(addr);
                auto            rc = ::getpeername(waiter_->file_descriptor(), &addr, &addrlen);

                if (!rc)
                {
                    auto tmp = tcp_stream(get_engine(), std::move(waiter_));
                    waiter_.reset();

                    co_return tmp;
                }
                else
                {
                    /* Get error from error spillage... */
                    char ch;
                    rc          = ::read(waiter_->file_descriptor(), &ch, 1);
                    last_error_ = errno;
                }
            }
            else
            {
                /* Get error from error spillage... */
                char ch;
                auto rc = ::read(waiter_->file_descriptor(), &ch, 1);
                (void) rc;
                last_error_ = errno;
                co_return std::nullopt;
            }
        }
        else
        {
            last_error_ = errno;
            co_return std::nullopt;
        }
    }

}   // namespace zab
