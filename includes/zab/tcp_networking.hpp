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
 *  @file tcp_networking.hpp
 *
 */

#ifndef ZAB_TCP_NETWORKING_HPP_
#define ZAB_TCP_NETWORKING_HPP_

#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdint.h>

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/memory_type.hpp"
#include "zab/network_operation.hpp"
#include "zab/pause.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"
#include "zab/tcp_stream.hpp"

struct sockaddr_storage;

namespace zab {

    /**
     * @brief      This class allows for asynchronous server based socket operations.
     *
     * @details    The methods of the class are essentially asynchronous equivalents to
     *             socket(2), bind(2), listen(2) and accept(2)
     */
    class tcp_acceptor : public network_operation {

        public:

            /**
             * @brief      Constructs a new instance in an empty state.
             *
             *             If this constructor is used, use of `listen` and `accept`,
             *             will result in undefined behavior until `register_engine`
             *             is called with a valid engine.
             */
            tcp_acceptor() = default;

            /**
             * @brief      Constructs a new instance in an empty state but with an engine
             * registered.
             *
             * @param      _engine  The engine to register.
             *
             */
            tcp_acceptor(engine* _engine);

            tcp_acceptor(const tcp_acceptor& _copy) = delete;

            tcp_acceptor(tcp_acceptor&& _copy) = default;

            /**
             * @brief      Move Assignment operator.
             *
             * @param      _move  The acceptor to move.
             *
             * @return     The result of the assignment
             */
            tcp_acceptor&
            operator=(tcp_acceptor&& _move) = default;

            /**
             * @brief      Swap two acceptors.
             *
             * @param      _first   The first
             * @param      _second  The second
             */
            friend void
            swap(tcp_acceptor& _first, tcp_acceptor& _second) noexcept;

            /**
             * @brief      Destroys the acceptor, closing its socket.
             *
             */
            ~tcp_acceptor() = default;

            [[nodiscard]] bool
            listen(int _family, std::uint16_t _port, int _backlog) noexcept;

            template <MemoryType DataType = std::byte>
            [[nodiscard]] ZAB_ASYNC_RETURN(std::optional<tcp_stream<DataType>>) accept(
                struct sockaddr* _address,
                socklen_t*       _length,
                int              _flags = SOCK_CLOEXEC) noexcept
            {
                ::memset(&_address, 0, sizeof(_address));
                return co_awaitable(
                    [this, ret = pause_pack{}, _address, _length, _flags]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            set_cancel(&ret);
                            get_engine()
                                ->get_event_loop()
                                .accept(&ret, descriptor(), _address, _length, _flags);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            std::optional<tcp_stream<DataType>> stream;
                            set_cancel(nullptr);
                            if (ret.data_ >= 0) { stream.emplace(get_engine(), ret.data_); }
                            else
                            {
                                set_error(-ret.data_);
                            }

                            return stream;
                        }
                    });
            }
    };

    template <MemoryType DataType = std::byte>
    [[nodiscard]] inline ZAB_ASYNC_RETURN(tcp_stream) tcp_connect(
        engine*                  _engine,
        struct sockaddr_storage* _details,
        socklen_t                _size,
        event_loop::io_handle*   cancel_token_ = nullptr,
        int                      _sock_flags   = SOCK_STREAM | SOCK_CLOEXEC)
    {
        network_operation net_op(_engine);
        int               sd;

        if (sd = ::socket(_details->ss_family, _sock_flags, 0); sd < 0) [[unlikely]]
        {
            net_op.set_error(errno);
        }
        net_op.set_descriptor(sd);

        return co_awaitable(
            [net_op = std::move(net_op),
             ret    = pause_pack{},
             _details,
             _size,
             cancel_token_]<typename T>(T _handle) mutable noexcept
            {
                if constexpr (is_ready<T>()) { return net_op.peek_error() != 0; }
                else if constexpr (is_suspend<T>())
                {
                    ret.handle_ = _handle;
                    if (cancel_token_) { *cancel_token_ = &ret; }

                    net_op.get_engine()->get_event_loop().connect(
                        &ret,
                        net_op.descriptor(),
                        (sockaddr*) _details,
                        _size);
                }
                else if constexpr (is_resume<T>())
                {
                    net_op.set_cancel(nullptr);
                    if (ret.data_ == 0)
                    {
                        auto ds = net_op.descriptor();
                        net_op.clear_descriptor();
                        return tcp_stream<DataType>(net_op.get_engine(), ds);
                    }
                    else
                    {
                        tcp_stream<DataType> stream(
                            net_op.get_engine(),
                            network_operation::kNoDescriptor);
                        stream.set_error(-ret.data_);

                        return stream;
                    }
                }
            });
    }

}   // namespace zab

#endif /* ZAB_TCP_NETWORKING_HPP_ */
