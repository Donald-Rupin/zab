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
 *  @file event_loop.hpp
 *
 */

#ifndef ZAB_EVENT_LOOP_HPP_
#define ZAB_EVENT_LOOP_HPP_

#include <atomic>
#include <coroutine>
#include <deque>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <type_traits>

#include "zab/async_function.hpp"
#include "zab/event.hpp"
#include "zab/pause.hpp"
#include "zab/simple_future.hpp"
#include "zab/spin_lock.hpp"

struct io_uring;
struct iovec;
struct sockaddr;

namespace zab {

    /**
     * @brief      This class implements an asynchronous `epoll` based descriptor notification
     * service.
     */
    class alignas(hardware_constructive_interference_size) event_loop {

        public:

            using io_handle = pause_pack*;

            class descriptor;

            static constexpr auto kQueueSize = 18024;

            /**
             * @brief      Constructs a new instance that will register to this engine.
             *
             * @param      _engine  The engine.
             *
             */
            event_loop();

            /**
             * @brief      Destroys the object and cleans up the resources.
             */
            ~event_loop();

            simple_future<int>
            open_at(
                int                    _dfd,
                const std::string_view _path,
                int                    _flags,
                mode_t                 _mode,
                io_handle*             _cancle_token = nullptr) noexcept;

            void
            open_at(
                io_handle              _cancle_token,
                int                    _dfd,
                const std::string_view _path,
                int                    _flags,
                mode_t                 _mode) noexcept;

            simple_future<int>
            close(int _fd, io_handle* _cancle_token = nullptr) noexcept;

            void
            close(io_handle _cancle_token, int _fd) noexcept;

            simple_future<int>
            read(
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset,
                io_handle*           _cancle_token = nullptr) noexcept;

            void
            read(
                io_handle            _cancle_token,
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset) noexcept;

            simple_future<int>
            fixed_read(
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset,
                int                  _buf_index,
                io_handle*           _cancle_token = nullptr) noexcept;

            void
            fixed_read(
                io_handle            _cancle_token,
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset,
                int                  _buf_index) noexcept;

            simple_future<int>
            read_v(
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset,
                io_handle*          _cancle_token = nullptr) noexcept;

            void
            read_v(
                io_handle           _cancle_token,
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset) noexcept;

            simple_future<int>
            write(
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset,
                io_handle*                 _cancle_token = nullptr) noexcept;

            void
            write(
                io_handle                  _cancle_token,
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset) noexcept;

            simple_future<int>
            fixed_write(
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset,
                int                        _buf_index,
                io_handle*                 _cancle_token = nullptr) noexcept;

            void
            fixed_write(
                io_handle                  _cancle_token,
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset,
                int                        _buf_index) noexcept;

            simple_future<int>
            write_v(
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset,
                io_handle*          _cancle_token = nullptr) noexcept;

            void
            write_v(
                io_handle           _cancle_token,
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset) noexcept;

            simple_future<int>
            recv(
                int                  sockfd,
                std::span<std::byte> _buffer,
                int                  _flags,
                io_handle*           _cancle_token = nullptr) noexcept;

            void
            recv(
                io_handle            _cancle_token,
                int                  sockfd,
                std::span<std::byte> _buffer,
                int                  _flags) noexcept;

            simple_future<int>
            send(
                int                        sockfd,
                std::span<const std::byte> _buffer,
                int                        _flags,
                io_handle*                 _cancle_token = nullptr) noexcept;

            void
            send(
                io_handle                  _cancle_token,
                int                        sockfd,
                std::span<const std::byte> _buffer,
                int                        _flags) noexcept;

            simple_future<int>
            accept(
                int              _fd,
                struct sockaddr* _addr,
                socklen_t*       _addrlen,
                int              _flag_,
                io_handle*       _cancle_token = nullptr) noexcept;

            void
            accept(
                io_handle        _cancle_token,
                int              _fd,
                struct sockaddr* _addr,
                socklen_t*       _addrlen,
                int              _flag_) noexcept;

            simple_future<int>
            connect(
                int              _fd,
                struct sockaddr* _addr,
                socklen_t        _addrlen,
                io_handle*       _cancle_token = nullptr) noexcept;

            void
            connect(
                io_handle        _cancle_token,
                int              _fd,
                struct sockaddr* _addr,
                socklen_t        _addrlen) noexcept;

            enum class CancelResults {
                kDone,
                kNotFound,
                kTried,
                kFailed,
                kUnknown
            };

            guaranteed_future<CancelResults>
            cancel_event(
                io_handle      _key,
                bool           _resume      = false,
                std::uintptr_t _cancel_code = std::numeric_limits<std::uintptr_t>::max() -
                                              1) noexcept;

            static void
            clean_up(
                io_handle      _key,
                bool           _resume      = false,
                std::uintptr_t _cancel_code = std::numeric_limits<std::uintptr_t>::max() -
                                              1) noexcept;

            void
            submit_pending_events() noexcept;

            void
            user_event(event _handle) noexcept;

            inline std::size_t
            event_size() const noexcept
            {
                return size_.load(std::memory_order_relaxed);
            }

            inline auto
            get_stop_function() noexcept
            {
                return [this]()
                {
                    wake();
                };
            }

            void
            run(std::stop_token _st) noexcept;

            void
            wake();

            std::optional<std::pair<std::span<std::byte>, std::size_t>>
            claim_fixed_buffer();

            void
            release_fixed_buffer(std::pair<std::span<std::byte>, std::size_t> _buffer);

        private:

            async_function<>
            run_user_space(std::stop_token _st) noexcept;

            std::unique_ptr<io_uring> ring_;

            static constexpr int kPinSize      = 32767;
            static constexpr int kTotalBuffers = 1000;

            std::vector<std::byte>                           pinned_buffers_;
            std::deque<std::pair<std::span<std::byte>, int>> free_buffers_;

            static constexpr int kWriteIndex = 0;
            static constexpr int kReadIndex  = 1;

            int                      user_space_event_fd_;
            std::atomic<std::size_t> size_;
            spin_lock                mtx_;
            std::deque<event>        handles_[2];
            io_handle                use_space_handle_;
    };

}   // namespace zab

#endif /* ZAB_EVENT_LOOP_HPP_ */