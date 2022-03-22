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
     * @brief      This class represents the a duplex network stream for writing and reading
     data.
     *
     */
    template <MemoryType DataType = std::byte>
    class tcp_stream {

        public:

            static constexpr auto kMaxWrite = std::numeric_limits<std::uint16_t>::max();
            static constexpr auto kMaxRead  = std::numeric_limits<std::int32_t>::max() - 2;

            tcp_stream() : write_cancel_(nullptr) { }

            tcp_stream(engine* _engine, int _fd) : net_op_(_engine, _fd), write_cancel_(nullptr) { }

            tcp_stream(const tcp_stream& _copy) = delete;

            tcp_stream(tcp_stream&& _move) : tcp_stream() { swap(*this, _move); }

            ~tcp_stream()
            {
                if (write_cancel_) { background_cancel_write(); }
            }

            template <MemoryType DT>
            friend void
            swap(tcp_stream<DT>& _first, tcp_stream<DT>& _second) noexcept
            {
                using std::swap;
                swap(_first.net_op_, _second.net_op_);
                swap(_first.write_cancel_, _second.write_cancel_);
            }

            inline void
            register_engine(engine* _engine) noexcept
            {
                net_op_.register_engine(_engine);
            }

            [[nodiscard]] inline int
            descriptor() const noexcept
            {
                return net_op_.descriptor();
            }

            [[nodiscard]] inline int
            last_error() noexcept
            {
                return net_op_.last_error();
            }

            inline void
            set_error(int _error) noexcept
            {
                net_op_.set_error(_error);
            }

            [[nodiscard]] ZAB_ASYNC_RETURN(void) cancel_read(
                bool          _resume,
                std::intptr_t _return_code = std::numeric_limits<std::intptr_t>::min()) noexcept
            {
                return net_op_.cancel(_resume, _return_code);
            }

            [[nodiscard]] ZAB_ASYNC_RETURN(void) cancel_write(
                bool          _resume,
                std::intptr_t _return_code = std::numeric_limits<std::intptr_t>::min()) noexcept
            {
                return network_operation::cancel(
                    net_op_.get_engine(),
                    write_cancel_,
                    _resume,
                    _return_code);
            }

            [[nodiscard]] ZAB_ASYNC_RETURN(bool) close() noexcept
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

            [[nodiscard]] simple_future<>
            shutdown(
                bool          _resume      = false,
                std::intptr_t _return_code = std::numeric_limits<std::intptr_t>::min()) noexcept
            {
                co_await cancel_read(_resume, _return_code);
                co_await cancel_write(_resume, _return_code);

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

            [[nodiscard]] ZAB_ASYNC_RETURN(std::intptr_t)
                read_some(std::span<DataType> _data, size_t _offset = 0, int _flags = 0) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _data, _offset, _flags]<typename T>(
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
                            if (ret.data_ > 0) { return ret.data_; }
                            else
                            {
                                net_op_.set_error(ret.data_);
                                return std::intptr_t{-1};
                            }
                        }
                    });
            }

            [[nodiscard]] guaranteed_future<std::size_t>
            read(std::span<DataType> _data, size_t _offset = 0, int _flags = 0) noexcept
            {
                std::size_t so_far = _offset;
                while (so_far != _data.size())
                {
                    auto size = co_await read_some(_data, so_far, _flags);

                    if (size > 0) { so_far += size; }
                    else
                    {
                        break;
                    }
                }

                co_return so_far;
            }

            [[nodiscard]] auto
            write_some(std::span<const DataType> _data, size_t _offset = 0) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _data, _offset]<typename T>(
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
                            if (ret.data_ > 0) { return ret.data_; }
                            else
                            {
                                net_op_.set_error(ret.data_);
                                return std::intptr_t{-1};
                            }
                        }
                    });
            }

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

            async_function<>
            background_cancel_write()
            {
                co_await cancel_write(false);
            }

            network_operation     net_op_;
            event_loop::io_handle write_cancel_;
    };

}   // namespace zab

#endif /* ZAB_TCP_STREAM_HPP_ */