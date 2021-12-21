
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
#include "zab/descriptor_notifications.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

struct sockaddr;

namespace zab {

    /**
     * @brief      This class represents the a duplex network stream for writing and reading data.
     *
     */
    class tcp_stream : public engine_enabled<tcp_stream> {

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
             *             is operating in non-blocking mode.
             *
             *             Its possible that this construct may fail, and users should
             *             check last_error to ensure the class was constructed properly.
             *             This is because registering the socket with the notification
             *             service may fail.
             *
             * @param      _engine  The engine to use.
             * @param[in]  _fd      The socket to use.
             */
            tcp_stream(engine* _engine, int _fd);

            /**
             * @brief      Constructs the stream bound to a socket found in descriptor_waiter.
             *
             * @details    This class is designed around the invariant that the socket being
             *             waited on is operating in non-blocking mode.
             *
             *             If _awaiter does not contain a value, the stream is considered
             *             to be in an empty state where use of any member functions
             *             except assignment is undefined behavior.
             *
             * @param      _engine  The engine to use.
             * @param[in]  _fd      The socket to use.
             *
             */
            tcp_stream(
                engine*                                            _engine,
                std::optional<descriptor_notification::notifier>&& _awaiter);

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
             * @brief      Move assign a stream.
             *
             * @details    This deconstructs `this` - see `~tcp_stream` for requirements.
             *             Moving a stream that is in use results in undefined behavior.
             *
             * @param      _move  The stream to move
             *
             * @return     The result of the assignment
             */
            tcp_stream&
            operator=(tcp_stream&& _move);

            /**
             * @brief      Destroys the stream.
             *
             * @details    It is recommended to await `Shutdown` and to ensure the reader has exited
             * before deconstruction of a stream. If the stream is in use when it is deconstructed,
             * this will result in undefined behavior.
             *
             *             If `Shutdown` is not awaited before deconstruction, the internal state of
             * the stream is deconstructed at a later time. Essentially, ownership of the internal
             * state is given to a background process that will attempt to do a similar thing to
             * `Shutdown`. The only issue is that this background process cannot guarantee that
             * awaiting writes or reads exit before deconstruction is taken place.
             *
             *             This also means that the sockets life time will linger past the
             * deconstruction of the stream as the background process attempts to gracefully socket
             * close.
             */
            ~tcp_stream();

            /**
             * @brief      Shutdown the stream.
             *
             * @details    The shut down process is:
             *             1) Wake any readers
             *             2) subscribe for any writers to finish
             *             3) Call ::shutdown(SHUT_WR) to notify client that we are shutting down
             *             4) Attempt to wait for the write buffer to be flushed
             *             5) Attempt to drain the read buffer and client to acknowledge shutdown
             *
             *             While we wake the reader, there is no guaranteed that the reader
             *             has returned and resumed the underlying coroutine. As such, `ShutDown()`
             *             by itself does not guaranty that the stream can be safely deconstructed.
             *             It guaranties that the stream is no longer alive, and any readers will be
             *             returning. To be safe, its easier to have the logical thread that is
             * reading to do the deconstruction itself.
             *
             *             For applications that require reliable delivery of data, the tcp_stream
             * tries its best to ensure that all data is delivered. It is highly
             * unlikely that some data is not sent during shutdown. Although, like that of any
             * socket programming, we cannot garrenty delivery to the client side application (only
             * that we tried to send it and try to ensure write buffers are flushed). Most
             * application level protocols will include some form of acknowledgement in the case of
             * required reliable delivery and this is above the scope of this class.
             *
             * @return     An awaitable the returns after all steps are complete.
             */
            [[nodiscard]] simple_future<>
            shutdown() noexcept;

            /**
             * @brief      Attempt to read some data from the stream.
             *
             * @details    This function will suspend the calling coroutine until _amount data is
             * read or an error occurs.
             *
             *             If the user is interested in any amount of data, specifying
             *             _amount is 0 (the default) will return when an amount of data has bee
             * read.
             *
             *             This function can return less then _amount when an error also occurred in
             * conjunction to the read.
             *
             *             If the user wants no timeout, specifying _timout is -1 (default) will
             * indicate no timeout. Although, a call to `CancelRead()` will immediately timeout the
             *             reader.
             *
             *             The user should ensure that this function has exited before
             * deconstruction.
             *
             * @param[in]  _amount  The amount to read.
             *
             * @return     The data read if successful, std::nullopt if an error.
             */
            [[nodiscard]] simple_future<std::vector<char>>
            read(
                size_t                                  _amount = 0,
                descriptor_notification::descriptor_op* _op     = nullptr) noexcept;

            [[nodiscard]] guaranteed_future<std::size_t>
            read(
                std::span<char>                         _data,
                descriptor_notification::descriptor_op* _op = nullptr) noexcept;

            [[nodiscard]] simple_future<std::vector<char>>
            read_some(size_t _max, descriptor_notification::descriptor_op* _op = nullptr) noexcept;

            [[nodiscard]] guaranteed_future<std::size_t>
            read_some(
                std::span<char>                         _data,
                descriptor_notification::descriptor_op* _op = nullptr) noexcept;

            /**
             * @brief      Write some data to the stream waiting for the data to make it to the
             *             OS write buffer.
             *
             * @details    This function is thread safe and the writes are performed atomically in
             *             order of acquisition of the internal lock. Each block of data in the
             * write is sent before the internal lock is released.
             *
             *             Atomicity is always guarantied, and ordering is guaranteed by the current
             *             implementation of async_mutex.
             *
             *             The life time of the data held by the span must last longer then the call
             *             to this function.
             *
             *             The data actually written may be different to the amount given due to
             *             a stream error or to many attempts fail due the OS write buffer is full.
             *             If the OS write buffer is full, last_error will be set to 0.
             *
             * @param[in]  _data  The view of the data to send.
             *
             * @return     The amount of data written.
             */
            [[nodiscard]] guaranteed_future<size_t>
            write(
                std::span<const char>                   _data,
                descriptor_notification::descriptor_op* _op = nullptr) noexcept;

            [[nodiscard]] guaranteed_future<std::unique_ptr<descriptor_notification::descriptor_op>>
            start_write_operation() noexcept;

            [[nodiscard]] guaranteed_future<std::unique_ptr<descriptor_notification::descriptor_op>>
            start_read_operation() noexcept;

            /**
             * @brief     Immediately cancles all operations.
             *
             *            There is a race condition between `read`/`write` and `cancel` if used in
             *            different threads. `cancel` will only affect the operations iff it is,
             *            called after the call to `read`/`write` has suspended. Otherwise, those
             * operations will not be cancelled.
             */
            void
            cancel() noexcept;

            /**
             * @brief      Get the last error from an operation.
             *
             * @details    This function is not thread safe, so the last error cant be accessed
             *             reliably in the presence of function use across multiple threads.
             *
             *             The last error applies to the last error assigned across all threads.
             *             When you go to check the error from an operation, another thread could
             *             assign a new error. This may not be an issue, since the error is most
             *             likely the same if its an socket error.
             *
             * @return     The last error.
             */
            inline int
            last_error() const noexcept
            {
                return state_->last_error_;
            }

            /**
             * @brief      Clears the last error.
             *
             * @details    This function is not thread safe, so the last error cant be written to
             *             reliably in the presence of function use across multiple threads.
             *
             */
            inline void
            clear_last_error() const noexcept
            {
                state_->last_error_ = 0;
            }

            /**
             * @brief      The internal state of the steam.
             */
            struct internal_state {

                    /**
                     * @brief      Constructs the state with an engine to use and a
                     * descriptor_waiter.
                     *
                     * @param      _desc    The description
                     */
                    internal_state(std::optional<descriptor_notification::notifier>&& _desc);

                    ~internal_state();

                    std::optional<descriptor_notification::notifier> socket_;

                    int last_error_;
            };

        private:

            /**
             * @brief      Flushes the state to a background process to await shutdown.
             *
             * @param      _engine  The engine to use.
             * @param      _state   The state to shutdown.
             *
             * @return     Nothing.
             */
            static async_function<>
            clean_up_and_forget(engine* _engine, std::unique_ptr<internal_state>&& _state) noexcept;

            /**
             * @brief      Attempts to gracefully shutdown the state.
             *
             * @param      _engine  The engine to use.
             * @param      _state   The state to shutdown.
             *
             * @return     An awaitable that returns after the shutdown attempt is complete.
             */
            [[nodiscard]] static simple_future<>
            clean_up(engine* _engine, std::unique_ptr<internal_state>&& _state) noexcept;

            std::unique_ptr<internal_state> state_;
    };

}   // namespace zab

#endif /* ZAB_STREAM_HPP_ */