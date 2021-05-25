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
 *  @file network_overlay.hpp
 *
 */

#ifndef ZAB_NETWORK_OVERLAY_HPP_
#define ZAB_NETWORK_OVERLAY_HPP_

#include <atomic>
#include <memory>
#include <span>
#include <stdint.h>
#include <sys/socket.h>

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/descriptor_notifications.hpp"
#include "zab/engine_enabled.hpp"
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
    class tcp_acceptor : public engine_enabled<tcp_acceptor> {

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

            /**
             * @brief      Move construct a new instance. The parameter is essentially swapped with
             *             default constructed instance.
             *
             * @param      _move  The tcp_acceptor to move.
             */
            tcp_acceptor(tcp_acceptor&& _move);

            /**
             * @brief      Cant copy it.
             *
             * @param[in]  _copy  The copy
             *
             */
            tcp_acceptor(const tcp_acceptor& _copy) = delete;

            /**
             * @brief      Move Assignment operator.
             *
             * @param      _move  The acceptor to move.
             *
             * @return     The result of the assignment
             */
            tcp_acceptor&
            operator=(tcp_acceptor&& _move);

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
            ~tcp_acceptor();

            /**
             * @brief      listen for incoming tcp connections of a given family,
             *             on a given port. The socket used will have `SO_REUSEADDR` set.
             *
             *             AF_INET and AF_INET6 are the valid values for family.
             *
             *             Port is given in host order endianness.
             *
             *             "The backlog argument defines the maximum length to which the
             *             queue of pending connections for sockfd may grow.  If a
             *             connection request arrives when the queue is full, the client may
             *             receive an error with an indication of ECONNREFUSED or, if the
             *             underlying protocol supports retransmission, the request may be
             *             ignored so that a later reattempt at connection succeeds." - listen(2)
             *
             *             last_error is set on failure.
             *
             * @param[in]  _family   The ip family.
             * @param[in]  _port     The port to bind too.
             * @param[in]  _backlog  The size of the tcp backlog.
             *
             * @return     true if successful, false otherwise.
             */
            [[nodiscard]] bool
            listen(int _family, std::uint16_t _port, int _backlog) noexcept;

            /**
             * @brief      Await for a connection to come in.
             *
             *             The timeout can be used to resume the suspended coroutine
             *             early. The number is in milliseconds and -1 means no timeout.
             *
             *             last_error is set on failure.
             *
             * @param[in]  _timeout  The timeout to use.
             *
             * @return     [through co_await] An optional tcp_stream that is set to the
             *             accepted connection on success, or std::nullopt on failure.
             *
             */
            [[nodiscard]] simple_future<tcp_stream>
            accept(int _timeout = -1) noexcept;

            /**
             * @brief      Retrieve the last value of errno recorded
             *             after the acceptor failed an operation.
             *
             *             This operation also clears the error code.
             *
             * @return     errno.
             */
            [[nodiscard]] inline int
            last_error() noexcept
            {
                auto tmp    = last_error_;
                last_error_ = 0;
                return tmp;
            }

        private:

            std::optional<descriptor_notification::descriptor_waiter> waiter_;
            int                                                       last_error_ = 0;
    };

    /**
     * @brief      This class allows for asynchronous client based socket operations.
     *
     * @details    The methods of the class are essentially asynchronous equivalents to
     *             socket(2) and connect(2)
     */
    class tcp_connector : public engine_enabled<tcp_connector> {

        public:

            /**
             * @brief      Constructs a new instance an an empty state.
             *
             *             If this constructor is used, use of `connect`,
             *             will result in undefined behavior until `register_engine`
             *             is called with a valid engine.
             */
            tcp_connector() = default;

            /**
             * @brief      Constructs a new instance in an empty state but with an engine
             * registered.
             *
             * @param      _engine  The engine to register.
             *
             */
            tcp_connector(engine* _engine);

            /**
             * @brief      Move construct a new instance. The parameter is essentially swapped with
             *             default constructed instance.
             *
             * @param      _move  The tcp_connector to move.
             */
            tcp_connector(tcp_connector&& _move);

            /**
             * @brief      Cant copy it.
             *
             * @param[in]  _copy  The copy
             *
             */
            tcp_connector(const tcp_connector& _copy) = delete;

            /**
             * @brief      Move Assignment operator.
             *
             * @param      _move  The acceptor to move.
             *
             * @return     The result of the assignment
             */
            tcp_connector&
            operator=(tcp_connector&&);

            /**
             * @brief      Swap two connectors.
             *
             * @param      _first   The first
             * @param      _second  The second
             */
            friend void
            swap(tcp_connector& _first, tcp_connector& _second) noexcept;

            /**
             * @brief      Destroys the connector, closing its socket.
             *
             */
            ~tcp_connector();

            /**
             * @brief      connect to a host.
             *
             *             The _details struct can be filled out using `getaddrinfo(3)` and
             *             passing in the `ai_addr` result member.
             *
             *             The timeout can be used to resume the suspended coroutine
             *             early. The number is in milliseconds and -1 means no timeout.
             *
             *             On success, the state of the connected is reset and can be reused.
             *
             *             last_error is set on failure.
             *
             * @param      _details  The host details.
             * @param      _size     The size of the _details struct
             * @param[in]  _timeout  The timeout to use.
             *
             * @return     [through co_await] An optional tcp_stream that is set to the
             *             accepted connection on success, or std::nullopt on failure.
             */
            [[nodiscard]] simple_future<tcp_stream>
            connect(struct sockaddr_storage* _details, socklen_t _size, int _timeout = -1) noexcept;

            /**
             * @brief      Retrieve the last value of errno recorded
             *             after the acceptor failed an operation.
             *
             *             This operation also clears the error code.
             *
             * @return     errno.
             */
            [[nodiscard]] inline int
            last_error() noexcept
            {
                auto tmp    = last_error_;
                last_error_ = 0;
                return tmp;
            }

        private:

            std::optional<descriptor_notification::descriptor_waiter> waiter_;
            int                                                       last_error_ = 0;
    };

}   // namespace zab

#endif /* ZAB_NETWORK_OVERLAY_HPP_ */
