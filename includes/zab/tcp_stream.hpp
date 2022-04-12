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

#ifndef ZAB_TCP_STREAM_HPP_
#define ZAB_TCP_STREAM_HPP_

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdint.h>

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/defer_block_promise.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/memory_type.hpp"
#include "zab/network_operation.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    /**
     * @brief This class represents the a duplex network stream for writing and reading
     *        data.
     *
     * @tparam DataType The MemoryType this stream reads and writes.
     */
    template <MemoryType DataType = std::byte>
    class tcp_stream {

        public:

            /**
             * @brief The maximum a write_some operation will write to a stream.
             *
             */
            static constexpr auto kMaxWrite = std::numeric_limits<std::uint16_t>::max();

            /**
             * @brief The maximum a read_some operation will write to a stream.
             *
             */
            static constexpr auto kMaxRead = std::numeric_limits<int>::max() - 2;

            /**
             * @brief Construct a tcp stream object in an empty state.
             *
             * @details Using this object from this constructor is undefined behaviour
             *          unless you are writing to it via a swap, a move or move assignment.
             */
            tcp_stream() : write_cancel_(nullptr) { }

            /**
             * @brief Construct a new tcp stream object associated with a engine and an socket
             * descriptor.
             *
             * @details This interface assumes the socket was created in blocking mode.
             *
             * @param _engine The engine to used.
             * @param _fd  The socket to used.
             */
            tcp_stream(engine* _engine, int _fd) : net_op_(_engine, _fd), write_cancel_(nullptr) { }

            /**
             * @brief There is no copy constructor for a tcp_stream.
             *
             */
            tcp_stream(const tcp_stream&) = delete;

            /**
             * @brief Construct a new tcp stream object by swapping the resources owned by
             *         _move.
             *
             * @param _move  The tcp stream to move.
             */
            tcp_stream(tcp_stream&& _move) : tcp_stream() { swap(*this, _move); }

            /**
             * @brief Destroy the tcp stream object.
             *
             * @details It is far more efficient to manually shutdown, cancel reads and writes
             *          and close the socket before the object is deconstructed. Failure to do so
             *          will spawn background fibres to do this in the background.
             *
             *          We first try to cancel any pending writes in the background.
             *          The clear any errors from the socket.
             *
             *          The deconstruction of net_op_ will cancel any pending reads
             *          and close the socket in the background.
             *
             */
            ~tcp_stream()
            {
                if (write_cancel_) { background_cancel_write(); }

                if (net_op_.descriptor())
                {
                    /* Clear any errors. */
                    int       result;
                    socklen_t result_len = sizeof(result);
                    ::getsockopt(
                        net_op_.descriptor(),
                        SOL_SOCKET,
                        SO_ERROR,
                        (char*) &result,
                        &result_len);
                }
            }

            /**
             * @brief Swaps two tcp streams.
             *
             * @tparam DT1 The memory type of the first one.
             * @tparam DT2 The memory type of the second one.
             * @param _first  The first stream.
             * @param _second The second stream.
             */
            template <MemoryType DT1, MemoryType DT2>
            friend void
            swap(tcp_stream<DT1>& _first, tcp_stream<DT2>& _second) noexcept
            {
                using std::swap;
                swap(_first.net_op_, _second.net_op_);
                swap(_first.write_cancel_, _second.write_cancel_);
            }

            /**
             * @brief Swaps two tcp streams.
             *
             * @tparam DT1 The memory type of the first one.
             * @param _first  The first stream.
             * @param _second The second stream.
             */
            template <MemoryType DT1>
            friend void
            swap(tcp_stream<DT1>& _first, tcp_stream<DT1>& _second) noexcept
            {
                using std::swap;
                swap(_first.net_op_, _second.net_op_);
                swap(_first.write_cancel_, _second.write_cancel_);
            }

            /**
             * @brief Move operator for a tcp stream. This will just swap to two streams.
             *
             * @param tcp_stream The stream to swap.
             * @return tcp_stream& this.
             */
            tcp_stream&
            operator=(tcp_stream&& tcp_stream)
            {
                swap(*this, tcp_stream);
                return *this;
            }

            /**
             * @brief Rebinds the memory type of the stream via a swap.
             *
             * @tparam DT1 The memory type of the resulting stream.
             * @tparam DT2 The memory type of the resulting to swap.
             * @param _other The stream to rebind.
             * @return tcp_stream<DT1> The newly typed stream.
             */
            template <MemoryType DT1, MemoryType DT2>
            friend tcp_stream<DT1>
            rebind(tcp_stream<DT2>& _other)
            {
                tcp_stream<DT1> new_stream;
                swap(new_stream, _other);

                return new_stream;
            }

            /**
             * @brief Get the underlying socket descriptor.
             *
             * @return int
             */
            [[nodiscard]] inline int
            descriptor() const noexcept
            {
                return net_op_.descriptor();
            }

            /**
             * @brief Get the last error.
             *
             * @details This clears the error.
             *
             * @return int
             */
            [[nodiscard]] inline int
            last_error() noexcept
            {
                return net_op_.last_error();
            }

            /**
             * @brief Set the error for the stream.
             *
             * @param _error The error to set for the stream.
             */
            inline void
            set_error(int _error) noexcept
            {
                net_op_.set_error(_error);
            }

            /**
             * @brief Attempt to cancel the current read operation.
             *
             * @details This function only suspends if there is a pending read operation.
             *
             * @co_return void Resumes after cancelation.
             */
            [[nodiscard]] auto
            cancel_read() noexcept
            {
                return net_op_.cancel();
            }

            /**
             * @brief Attempt to cancel the current write operation.
             *
             * @details This function only suspends if there is a pending write operation.
             *
             * @co_return void Resumes after cancelation.
             */
            [[nodiscard]] auto
            cancel_write() noexcept
            {
                return network_operation::cancel(net_op_.get_engine(), write_cancel_);
            }

            /**
             * @brief Attempt to close the socket.
             *
             * @details This function only suspends if there is a socket to close.
             *
             * @co_return true The socket was successfully closed.
             * @co_return false The socket failed to close.
             *
             */
            [[nodiscard]] auto
            close() noexcept
            {
                /* Clear any errors. */
                int       result;
                socklen_t result_len = sizeof(result);
                ::getsockopt(
                    net_op_.descriptor(),
                    SOL_SOCKET,
                    SO_ERROR,
                    (char*) &result,
                    &result_len);

                return net_op_.close();
            }

            /**
             * @brief Shutdown the stream by cancelling any pending operations and signalling
             * shutdown.
             *
             * @co_return void on shutdown completion.
             */
            [[nodiscard]] simple_future<>
            shutdown() noexcept
            {
                co_await cancel_read();
                co_await cancel_write();

                /* Intiate shut down */
                if (::shutdown(net_op_.descriptor(), SHUT_WR) != -1)
                {
                    /* Attempt to deplete read buffer and wait for client to ack */
                    std::vector<DataType> data(1028);
                    for (int i = 0; i < 5; ++i)
                    {
                        auto res = co_await read_some(data);
                        if (res <= 0)
                        {
                            /* Error or 0 is good here */
                            break;
                        }
                    }
                }
            }

            /**
             * @brief Attempt to read up to `_data.size() - _offset` bytes into the span at the
             *        given offset.
             *
             * @details If _data.size() - _offset is larger then kMaxRead, then kMaxRead is used as
             *          the max.
             *
             * @param _data The buffer to read into.
             * @param _offset The offset from where to start reading in.
             * @param _flags Any flags to pass to recv.
             * @co_return int The amount of bytes read or -1 if an error occurred.
             */
            [[nodiscard]] auto
            read_some(std::span<DataType> _data, size_t _offset = 0, int _flags = 0) noexcept
            {
                return co_awaitable(
                    [this, ret = io_handle{}, _data, _offset, _flags]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_ready<T>()) { return !_data.size(); }
                        else if constexpr (is_suspend<T>())
                        {
                            auto amount_to_read =
                                std::min<std::size_t>(_data.size() - _offset, kMaxRead);

                            ret.handle_ = _handle;
                            net_op_.set_cancel(&ret);
                            net_op_.get_engine()->get_event_loop().recv(
                                &ret,
                                net_op_.descriptor(),
                                std::span<std::byte>(
                                    (std::byte*) _data.data() + _offset,
                                    amount_to_read),
                                _flags);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            net_op_.clear_cancel();
                            if (ret.result_ > 0) { return ret.result_; }
                            else
                            {
                                net_op_.set_error(-ret.result_);
                                return -1;
                            }
                        }
                    });
            }

            /**
             * @brief Attempts to `_data.size() - _offset` bytes into the span at the given
             *        offset. Blocks until the amount is read, a signal interupts the call or an
             *        error occurs.
             *
             * @details If _data.size() - _offset is larger then kMaxRead, then kMaxRead is used as
             *          the max.
             *
             *
             * @param _data The buffer to read into.
             * @param _offset The offset from where to start reading in.
             * @param _flags Any flags to pass to recv.
             * @co_return std::size_t The amount of bytes read.
             */
            [[nodiscard]] guaranteed_future<std::size_t>
            read(std::span<DataType> _data, size_t _offset = 0, int _flags = 0) noexcept
            {
                std::size_t so_far = _offset;
                while (so_far != _data.size())
                {
                    auto result = co_await read_some(_data, so_far, _flags | MSG_WAITALL);
                    if (result > 0) { so_far += result; }
                    else
                    {
                        break;
                    }
                }

                co_return so_far;

                // return read_some(_data, _offset, _flags | MSG_WAITALL);
            }

            /**
             * @brief Attempt to write up to `_data.size() - _offset` bytes from the span at the
             *        given offset.
             *
             * @details If _data.size() - _offset is larger then kMaxWrite, then kMaxWrite is used
             *          as the max.
             *
             * @param _data The buffer to write from.
             * @param _offset The offset from where to start writing from.
             * @param _flags Any flags to pass to send. MSG_NOSIGNAL is always set additionally.
             * @co_return int The amount of bytes written or -1 if an error occurred.
             */
            [[nodiscard]] auto
            write_some(std::span<const DataType> _data, size_t _offset = 0) noexcept
            {
                return co_awaitable(
                    [this, ret = io_handle{}, _data, _offset]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_ready<T>()) { return !_data.size(); }
                        else if constexpr (is_suspend<T>())
                        {
                            auto amount_to_write =
                                std::min<std::size_t>(_data.size() - _offset, kMaxWrite);

                            ret.handle_   = _handle;
                            write_cancel_ = &ret;
                            net_op_.get_engine()->get_event_loop().send(
                                &ret,
                                net_op_.descriptor(),
                                std::span<std::byte>(
                                    (std::byte*) _data.data() + _offset,
                                    amount_to_write),
                                MSG_NOSIGNAL);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            write_cancel_ = nullptr;
                            if (ret.result_ > 0) { return ret.result_; }
                            else
                            {
                                net_op_.set_error(-ret.result_);
                                return -1;
                            }
                        }
                    });
            }

            /**
             * @brief Attempt to write up to `_data.size() - _offset` bytes from the span at the
             *        given offset. Blocks until the amount is written or an error occurs.
             *
             * @details If _data.size() - _offset is larger then kMaxWrite, then kMaxWrite is used
             *          as the max.
             *
             *          Coroutine chaining can be expensive exspecially if the call to `write()` is
             *          within a hotpath. As such, if the writing is not in a hotpath or are in not
             *          performance critical parts of the program `write()` is encrouaged for
             *          readability an maintainabilty. Otherwise authors are encoruged to use
             *          write_some and handle their own logic for when the incorrect amount is
             *          written.
             *
             * @param _data The buffer to write from.
             * @param _offset The offset from where to start writing from.
             * @param _flags Any flags to pass to send. MSG_NOSIGNAL is always set additionaly.
             * @co_return std::size_t The amount of bytes written.
             */
            [[nodiscard]] guaranteed_future<std::size_t>
            write(std::span<const DataType> _data, size_t _offset = 0) noexcept
            {
                std::size_t so_far = _offset;
                while (so_far != _data.size())
                {
                    auto result = co_await write_some(_data, so_far);
                    if (result > 0) { so_far += result; }
                    else
                    {
                        break;
                    }
                }

                co_return so_far;
            }

        private:

            /**
             * @brief Cancel any pending writes in the background.
             *
             * @return async_function<>
             */
            async_function<>
            background_cancel_write()
            {
                co_await cancel_write();
            }

            network_operation net_op_;
            io_handle*        write_cancel_;
    };

}   // namespace zab

#endif /* ZAB_TCP_STREAM_HPP_ */