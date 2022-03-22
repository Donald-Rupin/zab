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
     * @brief      This class implements a coroutine wrapper for the liburing service.
     */
    class alignas(hardware_constructive_interference_size) event_loop {

        public:

            using io_handle = pause_pack*;

            static constexpr auto kQueueSize = 4096;

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

            /**
             * @brief Open a file relative to a directory file descriptor
             *
             * @details See https://linux.die.net/man/2/openat.
             *
             * @param _dfd The directory descriptor or AT_FDCWD
             * @param _path The path to the file relative to _dfd.
             * @param _flags Flags used to open the file.
             * @param _mode  Permissions to use for the file
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which opens a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            open_at(
                int                    _dfd,
                const std::string_view _path,
                int                    _flags,
                mode_t                 _mode,
                io_handle*             _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this,
                     ret = pause_pack{},
                     _dfd,
                     _path,
                     _flags,
                     _mode,
                     _cancle_token]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            open_at(&ret, _dfd, _path, _flags, _mode);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Open a file relative to a directory file descriptor
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://linux.die.net/man/2/openat.
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _dfd The directory descriptor or AT_FDCWD
             * @param _path The path to the file relative to _dfd.
             * @param _flags Flags used to open the file.
             * @param _mode  Permissions to use for the file
             *
             */
            void
            open_at(
                io_handle              _cancle_token,
                int                    _dfd,
                const std::string_view _path,
                int                    _flags,
                mode_t                 _mode) noexcept;

            /**
             * @brief Close a file descriptor
             *
             * @details See https://man7.org/linux/man-pages/man2/close.2.html.
             *
             * @param _fd The file descriptor to close.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which closes a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            close(int _fd, io_handle* _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _fd, _cancle_token]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            close(&ret, _fd);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Close a file descriptor
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/close.2.html.
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _fd The file descriptor to close.
             * .
             */
            void
            close(io_handle _cancle_token, int _fd) noexcept;

            /**
             * @brief Read from a file descriptor.
             *
             * @details See https://man7.org/linux/man-pages/man2/read.2.html.
             *
             * @param _fd The file to read from.
             * @param _buffer The buffer to read into.
             * @param _offset The offset of the buffer to start from.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which reads from a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            read(
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset,
                io_handle*           _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _fd, _buffer, _offset, _cancle_token]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            read(&ret, _fd, _buffer, _offset);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Read from a file descriptor.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/read.2.html.
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _fd The file to read from.
             * @param _buffer The buffer to read into.
             * @param _offset The offset of the buffer to start from.
             *
             */
            void
            read(
                io_handle            _cancle_token,
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset) noexcept;

            /**
             * @brief Read from a file descriptor using a fixed buffer.
             *
             * @details See https://man7.org/linux/man-pages/man2/read.2.html.
             *
             * @param _fd The file to read from.
             * @param _buffer The buffer to read into.
             * @param _offset The offset of the buffer to start from.
             * @param _buf_index The index of the fixed buffer.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which reads from a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            fixed_read(
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset,
                int                  _buf_index,
                io_handle*           _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this,
                     ret = pause_pack{},
                     _fd,
                     _buffer,
                     _offset,
                     _buf_index,
                     _cancle_token]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            fixed_read(&ret, _fd, _buffer, _offset, _buf_index);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Read from a file descriptor using a fixed buffer.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/read.2.html.
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _fd The file to read from.
             * @param _buffer The buffer to read into.
             * @param _offset The offset of the buffer to start from.
             * @param _buf_index The index of the fixed buffer.
             *
             */
            void
            fixed_read(
                io_handle            _cancle_token,
                int                  _fd,
                std::span<std::byte> _buffer,
                off_t                _offset,
                int                  _buf_index) noexcept;

            /**
             * @brief Read from a file descriptor into multiple buffers.
             *
             * @details See https://man7.org/linux/man-pages/man2/readv.2.html
             *
             * @param _fd The file to read from.
             * @param _iovecs The _iovecs array.
             * @param _nr_vecs The size of the _iovecs.
             * @param _offset The offset of the buffer to start from.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which reads from a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            read_v(
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset,
                io_handle*          _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this,
                     ret = pause_pack{},
                     _fd,
                     _iovecs,
                     _nr_vecs,
                     _offset,
                     _cancle_token]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            read_v(&ret, _fd, _iovecs, _nr_vecs, _offset);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Read from a file descriptor into multiple buffers.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/readv.2.html
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _fd The file to read from.
             * @param _iovecs The _iovecs array.
             * @param _nr_vecs The size of the _iovecs.
             * @param _offset The offset of the buffer to start from.
             *
             */
            void
            read_v(
                io_handle           _cancle_token,
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset) noexcept;

            /**
             * @brief Write to a file descriptor.
             *
             * @details See https://man7.org/linux/man-pages/man2/write.2.html.
             *
             * @param _fd The file descriptor to write to.
             * @param _buffer The buffer to write from.
             * @param _offset The offest from where to start writing.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which writes to a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            write(
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset,
                io_handle*                 _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _fd, _buffer, _offset, _cancle_token]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            write(&ret, _fd, _buffer, _offset);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Write to a file descriptor.
             *
             * @details @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/write.2.html
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _fd The file descriptor to write to.
             * @param _buffer The buffer to write from.
             * @param _offset The offest from where to start writing.
             *
             */
            void
            write(
                io_handle                  _cancle_token,
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset) noexcept;

            /**
             * @brief Write from a fixed buffer into a file descriptor.
             *
             * @details See https://man7.org/linux/man-pages/man2/write.2.html;
             *
             * @param _fd The file desciptor.
             * @param _buffer The buffer to write from.
             * @param _offset The offset of the buffer.
             * @param _buf_index The pinned buffer index.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which writes to a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            fixed_write(
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset,
                int                        _buf_index,
                io_handle*                 _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this,
                     ret = pause_pack{},
                     _fd,
                     _buffer,
                     _offset,
                     _buf_index,
                     _cancle_token]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            fixed_write(&ret, _fd, _buffer, _offset, _buf_index);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Write from a fixed buffer into a file descriptor.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/write.2.html
             *
             * @param _cancle_token A io_handle which will be resumed on completion.
             * @param _fd The file desciptor.
             * @param _buffer The buffer to write from.
             * @param _offset The offset of the buffer.
             * @param _buf_index The pinned buffer index.
             *
             */
            void
            fixed_write(
                io_handle                  _cancle_token,
                int                        _fd,
                std::span<const std::byte> _buffer,
                off_t                      _offset,
                int                        _buf_index) noexcept;

            /**
             * @brief Write data from multiple buffers.
             *
             * @details See: https://man7.org/linux/man-pages/man2/writev.2.html.
             *
             * @param _fd The file desciptor.
             * @param _iovecs The iovec structure array.
             * @param _nr_vecs The size of the iovec array.
             * @param _offset The offest from where to start writing.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which writes to a file.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            write_v(
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset,
                io_handle*          _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this,
                     ret = pause_pack{},
                     _fd,
                     _iovecs,
                     _nr_vecs,
                     _offset,
                     _cancle_token]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            write_v(&ret, _fd, _iovecs, _nr_vecs, _offset);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Write data from multiple buffers.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/writev.2.html
             *
             * @param _cancle_token  A io_handle which will be resumed on completion.
             * @param _fd The file desciptor.
             * @param _iovecs The iovec structure array.
             * @param _nr_vecs The size of the iovec array.
             * @param _offset The offest from where to start writing.
             *
             */
            void
            write_v(
                io_handle           _cancle_token,
                int                 _fd,
                const struct iovec* _iovecs,
                unsigned            _nr_vecs,
                off_t               _offset) noexcept;

            /**
             * @brief Receive a message from a socket.
             *
             * @details See https://man7.org/linux/man-pages/man2/recv.2.html.
             *
             * @param _sockfd The socket descriptor.
             * @param _buffer The buffer to receive into.
             * @param _flags The flags to apply to the read operation.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which receives a message.
             *         Async returns the return code of the operation.
             *
             */
            ZAB_ASYNC_RETURN(int)
            recv(
                int                  _sockfd,
                std::span<std::byte> _buffer,
                int                  _flags,
                io_handle*           _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _sockfd, _buffer, _flags, _cancle_token]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            recv(&ret, _sockfd, _buffer, _flags);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Receive a message from a socket.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/recv.2.html.
             *
             * @param _cancle_token  A io_handle which will be resumed on completion.
             * @param _sockfd The socket descriptor.
             * @param _buffer The buffer to receive into.
             * @param _flags The flags to apply to the read operation.
             *
             */
            void
            recv(
                io_handle            _cancle_token,
                int                  _sockfd,
                std::span<std::byte> _buffer,
                int                  _flags) noexcept;

            /**
             * @brief Send a message on a socket.
             *
             * @details: See https://man7.org/linux/man-pages/man2/send.2.html.
             *
             * @param _sockfd The socket descriptor.
             * @param _buffer The buffer to receive into.
             * @param _flags The flags to apply to the write operation.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which sends a message.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            send(
                int                        _sockfd,
                std::span<const std::byte> _buffer,
                int                        _flags,
                io_handle*                 _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _sockfd, _buffer, _flags, _cancle_token]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            send(&ret, _sockfd, _buffer, _flags);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Send a message on a socket.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/send.2.html.
             *
             * @param _cancle_token  A io_handle which will be resumed on completion.
             * @param _sockfd The socket descriptor.
             * @param _buffer The buffer to receive into.
             * @param _flags The flags to apply to the write operation.
             *
             */
            void
            send(
                io_handle                  _cancle_token,
                int                        _sockfd,
                std::span<const std::byte> _buffer,
                int                        _flags) noexcept;

            /**
             * @brief Accept a connection on a socket.
             *
             * @details See https://man7.org/linux/man-pages/man2/accept.2.html.
             *
             * @param _sockfd The socket descriptor.
             * @param _addr The sockaddr to use.
             * @param _addrlen The length of the sockaddre region.
             * @param _flags The flags to apply to the accept operation.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the
             *                           cancelation handle.
             *
             * @return A co_awaitable which accepts from a socket.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            accept(
                int              _sockfd,
                struct sockaddr* _addr,
                socklen_t*       _addrlen,
                int              _flags,
                io_handle*       _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this,
                     ret = pause_pack{},
                     _sockfd,
                     _addr,
                     _addrlen,
                     _flags,
                     _cancle_token]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            accept(&ret, _sockfd, _addr, _addrlen, _flags);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Accept a connection on a socket.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/accept.2.html.
             *
             *  @param _cancle_token  A io_handle which will be resumed on completion.
             * @param _sockfd The socket descriptor.
             * @param _addr The sockaddr to use.
             * @param _addrlen The length of the sockaddre region.
             * @param _flags The flags to apply to the accept operation.
             *
             */
            void
            accept(
                io_handle        _cancle_token,
                int              _sockfd,
                struct sockaddr* _addr,
                socklen_t*       _addrlen,
                int              _flag_) noexcept;

            /**
             * @brief Initiate a connection on a socket.
             *
             *  @details See https://man7.org/linux/man-pages/man2/connect.2.html.
             *
             * @param _sockfd The socket descriptor.
             * @param _addr The sockaddr to use.
             * @param _addrlen The length of the sockaddre region.
             * @param[out] _cancle_token A ptr to a io_handle which will bet set to the cancelation
             *                          handle.
             *
             *
             * @return A co_awaitable which connects from a socket.
             *         Async returns the return code of the operation.
             */
            ZAB_ASYNC_RETURN(int)
            connect(
                int              _sockfd,
                struct sockaddr* _addr,
                socklen_t        _addrlen,
                io_handle*       _cancle_token = nullptr) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _sockfd, _addr, _addrlen, _cancle_token]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            if (_cancle_token) { *_cancle_token = &ret; }
                            connect(&ret, _sockfd, _addr, _addrlen);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return ret.data_;
                        }
                    });
            }

            /**
             * @brief Initiate a connection on a socket.
             *
             * @details _cancle_token->data_ will hold the return code of the op.
             *
             *          See https://man7.org/linux/man-pages/man2/connect.2.html.
             *
             * @param _cancle_token  A io_handle which will be resumed on completion.
             * @param _sockfd The socket descriptor.
             * @param _addr The sockaddr to use.
             * @param _addrlen The length of the sockaddre region.
             */
            void
            connect(
                io_handle        _cancle_token,
                int              _sockfd,
                struct sockaddr* _addr,
                socklen_t        _addrlen) noexcept;

            /**
             * @brief Describes the result of a cancel operation.
             *
             */
            enum class CancelResult {
                kDone,     /**< The cancel was complete. */
                kNotFound, /**< Could not find an operation with that io_handle. */
                kTried,    /**< We tried, but the operation could not be canceled. */
                kFailed,   /**< We could not create the cancel request dues to an error. */
                kUnknown   /**< Something exception and unknown happened. */
            };

            /**
             * @brief Conerts errno error codes into CancelResult.
             *
             * @param _result The errno result.
             * @return CancelResult The corresponding CancelResult.
             */
            static CancelResult
            cancel_code(std::intptr_t _result) noexcept;

            /**
             * @brief Attempt to cancel an operation.
             *
             * @details See `CancelResult` for possible results.
             *
             * @param _key The io handle to cancel.
             * @param _resume Whether to resume or destroy the io_handle.
             * @param _cancel_code On resumption, the code to resume the io_handle with.
             *
             * @return A co_awaitable which connects cancels and operation.
             *         Async returns the CancelResult.
             *
             */
            ZAB_ASYNC_RETURN(CancelResult)
            cancel_event(
                io_handle     _key,
                bool          _resume      = false,
                std::intptr_t _cancel_code = std::numeric_limits<std::intptr_t>::max() - 1) noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}, _key, _resume, _cancel_code]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            cancel_event(&ret, _key);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            auto result = cancel_code(ret.data_);

                            if (result == CancelResult::kDone)
                            {
                                clean_up(_key, _resume, _cancel_code);
                            }

                            return result;
                        }
                    });
            }

            /**
             * @brief Intiate a cancel operation.
             *
             * @details This cancel does not attempt to clean up the
             *          waiting request.
             *
             * @param _cancle_token The handle to resume once cancel has been attempted.
             * @param _key The handle to cancel.
             */
            void
            cancel_event(io_handle _cancle_token, io_handle _key) noexcept;

            /**
             * @brief Cleans up an io_handle be either resuming or destroying it.
             *
             * @param _key The io_handle.
             * @param _resume true if resume, false if destroy.
             * @param _cancel_code If resume, what code to pass to the coroutine.
             */
            static void
            clean_up(
                io_handle      _key,
                bool           _resume      = false,
                std::uintptr_t _cancel_code = std::numeric_limits<std::uintptr_t>::max() -
                                              1) noexcept;

            /**
             * @brief Updater the counter on the submission queue to include any new
             *        submission.
             *
             * @details This is done on every pass of the completion qeue.
             *
             */
            void
            submit_pending_events() noexcept;

            /**
             * @brief Submits a user event to the event_loop.
             *
             * @param _handle The event to submit.
             */
            void
            user_event(event _handle) noexcept;

            /**
             * @brief The number of user events currently waiting to be handled.
             *
             * @return std::size_t
             */
            inline std::size_t
            event_size() const noexcept
            {
                return size_.load(std::memory_order_relaxed);
            }

            /**
             * @brief Get the stop function for the run time.
             *
             * @details This is a lambda that just calls wake.
             *
             * @return A lambda wrapper for wake.
             */
            inline auto
            get_stop_function() noexcept
            {
                return [this]()
                {
                    wake();
                };
            }

            /**
             * @brief Run the event loop until until signaled be the stop token.
             *
             * @param _st The stop token.
             */
            void
            run(std::stop_token _st) noexcept;

            /**
             * @brief Wakes the event loop by inserting a user space event.
             *
             */
            void
            wake();

            /**
             * @brief Claim a fixed buffer from the event loop for io operations.
             *
             * @details Can fail if there are no more fixed buffers left.
             *
             * @return std::optional<std::pair<std::span<std::byte>, std::size_t>>
             */
            std::optional<std::pair<std::span<std::byte>, std::size_t>>
            claim_fixed_buffer();

            /**
             * @brief Release a fixed buffer back to the event loop.
             *
             * @param _buffer  The buffer to release.
             */
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