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
 *  @file tcp_networking.cpp
 *
 */

#include "zab/tcp_networking.hpp"

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "zab/event_loop.hpp"
#include "zab/first_of.hpp"
#include "zab/timer_service.hpp"

namespace zab {

    tcp_acceptor::tcp_acceptor(engine* _engine) : network_operation(_engine) { }

    void
    swap(tcp_acceptor& _first, tcp_acceptor& _second) noexcept
    {
        using std::swap;
        swap(static_cast<network_operation&>(_first), static_cast<network_operation&>(_second));
    }

    bool
    tcp_acceptor::listen(int _family, std::uint16_t _port, int _backlog) noexcept
    {
        if (descriptor() < 0)
        {
            auto acc = ::socket(_family, SOCK_STREAM | SOCK_CLOEXEC, 0);

            if (acc < 0) [[unlikely]]
            {
                set_error(errno);
                return false;
            }

            set_descriptor(acc);
        }

        unsigned long on = 1;
        if (::setsockopt(descriptor(), SOL_SOCKET, SO_REUSEADDR, (char*) &on, sizeof(on)) != 0)
            [[unlikely]]
        {
            set_error(errno);
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
            set_error(EINVAL);
            return false;
        }

        if (::bind(descriptor(), (struct sockaddr*) &add, sizeof(add)) != 0) [[unlikely]]
        {
            set_error(errno);
            return false;
        }

        if (::listen(descriptor(), _backlog) != 0) [[unlikely]]
        {
            set_error(errno);
            return false;
        }

        return true;
    }

}   // namespace zab
