
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
 *  @file tcp_strean.hpp
 *
 */

#ifndef ZAB_STREAM_HPP_
#define ZAB_STREAM_HPP_

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <stdint.h>

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/defer_block_promise.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

struct sockaddr;

namespace zab {

    /**
     * @brief      This class represents the a duplex network stream for writing and reading
     data.
     *
     */
    class tcp_stream {

        public:

            /**
             * @brief      Constructs the stream an empty state. Use of any member functions
             *             except assignment is undefined behavior.
             *
             */
            tcp_stream() = default;

            /**
             * @brief      Constructs the stream bound to a socket referred to by _fd.
             *
             * @details    This class is designed around the invariant that the socket
             *             is operating in blocking mode.
             *
             *
             * @param      _engine  The engine to use.
             * @param[in]  _fd      The socket to use.
             */
            tcp_stream(engine* _engine, int _fd);

            /**
             * @brief      Cannot copy a stream.
             *
             * @param[in]  _copy  The copy
             */
            tcp_stream(const tcp_stream& _copy) = delete;

            /**
             * @brief      Move construct a stream.
             *
             * @details    Moving a stream that is in use results in undefined behavior.
             *
             * @param      _move  The move
             */
            tcp_stream(tcp_stream&& _move);

            /**
             * @brief      Destroys the stream.
             *
             * @details    The user needs to ensure oprations have exited before deconsturction.
             *             If the stream is in use when it is deconstructed, this will result in
             *             undefined behavior.
             *
             *             The user should also await `shutdown` before deconstruction of a
             stream.
             *
             *             If `shutdown` is not awaited before deconstruction, the internal state
             of
             *             the stream is deconstructed at a later time. Essentially, ownership of
             *             the internal state is given to a background fibre that will attempt to
             *             do a similar thing to `shutdown`. This means that the sockets life
             time
             *             will linger past the deconstruction of the stream as the background
             *             process attempts to gracefully socket close.
             */
            ~tcp_stream();

            /**
             * @brief      Swap two connectors.
             *
             * @param      _first   The first
             * @param      _second  The second
             */
            friend void
            swap(tcp_stream& _first, tcp_stream& _second) noexcept;

            /**
             * @brief      Shutdown the stream.
             *
             * @details    This requires that no reads or writes are in progress.
             *             The shutdown process is:
             *             1) Destroy the internal state.
             *             2) Call ::shutdown(SHUT_WR) to notify client that we are shutting down
             *             3) Attempt to wait for the write buffer to be flushed
             *             4) Attempt to drain the read buffer and client to acknowledge shutdown
             *
             *             For applications that require reliable delivery of data, the
             tcp_stream
             *             tries its best to ensure that all data is delivered. The user should
             *             wait for all writes to return. Although, like that of any socket
             *             programming, we cannot garrenty delivery to the client side
             application
             *             (only that we tried to send it and try to ensure write buffers are
             *             flushed). Most application level protocols will include some form of
             *             acknowledgement in the case of required reliable delivery and this is
             *             above the scope of this class.
             *
             * @return     An awaitable that returns after all steps are complete.
             */
            [[nodiscard]] simple_future<>
            shutdown() noexcept;

            /**
             * @brief      Attempt to read data from the stream.
             *
             * @details    This function will suspend the calling coroutine until _amount data is
             *             read, an error occurs or it is cancelled.
             *
             *             This function can return less then _amount when an error ro cancel
             *             occurs.
             *
             *             The user should ensure that this function has exited before
             *             deconstruction.
             *
             * @param[in]  _amount  The amount to read.
             *
             * @return     The data read if successful, std::nullopt if an error.
             */
            [[nodiscard]] simple_future<std::vector<char>>
            read(size_t _amount) noexcept;

            /**
             * @brief      Attempt to read data from the stream.
             *
             * @details    This function will suspend the calling coroutine until _data.size()
             data
             *             is read or an error occurs.
             *
             *             This function can return less then _amount when an error ro cancel
             *             occurs.
             *
             *             The user should ensure that this function has exited before
             *             deconstruction.
             *
             * @param[in]  _amount  The amount to read.
             *
             * @return     The data read if successful, std::nullopt if an error.
             */
            [[nodiscard]] guaranteed_future<std::size_t>
            read(std::span<char> _data) noexcept;

            /**
             * @brief      Attempt to read up _max data from the stream.
             *
             * @details    This function will suspend the calling coroutine until some data
             *             is read or an error occurs.
             *
             *
             *             The user should ensure that this function has exited before
             *             deconstruction.
             *
             * @param[in]  _max  The amount to read.
             *
             * @return     The data read if successful, std::nullopt if an error.
             */
            [[nodiscard]] simple_future<std::vector<char>>
            read_some(size_t _max) noexcept;

            /**
             * @brief      Attempt to read up _data.size() data from the stream.
             *
             * @details    This function will suspend the calling coroutine until some data
             *             is read or an error occurs.
             *
             *             The user should ensure that this function has exited before
             *             deconstruction.
             *
             * @param[in]  _max  The amount to read.
             *
             * @return     The data read if successful, std::nullopt if an error.
             */
            [[nodiscard]] guaranteed_future<std::size_t>
            read_some(std::span<char> _data) noexcept;

            /**
             * @brief      Write some data to the stream waiting for the data to make it to the
             *             OS write buffer.
             *
             * @details    The life time of the data held by the span must last longer then the
             call
             *             to this function.
             *
             *             The data actually written may be different to the amount given due to
             *             a stream error or cancellation.
             *
             *             Calls to writes are not atomic.
             *
             * @param[in]  _data  The view of the data to send.
             *
             * @return     The amount of data written.
             */
            [[nodiscard]] guaranteed_future<std::size_t>
            write(std::span<const char> _data) noexcept;

            [[nodiscard]] guaranteed_future<std::size_t>
            write_fixed(std::span<const std::byte> _data, int _index) noexcept;

            [[nodiscard]] guaranteed_future<std::size_t>
            read_fixed(std::span<std::byte> _data, int _index) noexcept;

            struct op_control {
                    std::byte*  data_;
                    std::size_t size_;
            };

            [[nodiscard]] reusable_future<std::size_t>
            get_reader(op_control* _oc) noexcept;

            [[nodiscard]] reusable_future<std::size_t>
            get_writer(op_control* _oc) noexcept;

            /**
             * @brief     Immediately cancles all operations.
             *
             */
            simple_future<bool>
            cancel() noexcept;

            /**
             * @brief      Get the last error from an operation.
             *
             * @return     The last error.
             */
            inline int
            last_error() const noexcept
            {
                return last_error_;
            }

            /**
             * @brief      Clears the last error.
             *
             */
            inline void
            clear_last_error() noexcept
            {
                last_error_ = 0;
            }

        private:

            engine*               engine_       = nullptr;
            event_loop::io_handle cancel_token_ = nullptr;
            int                   connection_   = 0;
            int                   last_error_   = 0;
    };

}   // namespace zab

#endif /* ZAB_STREAM_HPP_ */