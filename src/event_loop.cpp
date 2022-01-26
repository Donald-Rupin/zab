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
 *  @file event_loop.cpp
 *
 */

#include "zab/event_loop.hpp"

#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <liburing.h>
#include <memory>
#include <mutex>
#include <optional>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

#include "zab/strong_types.hpp"

namespace zab {

    namespace {

        template <typename FunctionCallType, FunctionCallType Function, typename... Args>
        inline void
        do_op_impl(event_loop::io_handle _cancle_token, struct io_uring* _ring, Args&&... _args)
        {
            auto* sqe = io_uring_get_sqe(_ring);

            if (sqe)
            {
                (*Function)(sqe, std::forward<Args>(_args)...);

                io_uring_sqe_set_data(sqe, _cancle_token);
            }
            else
            {
                _cancle_token->data_ = -1;
                _cancle_token->handle_.resume();
            }
        }

        /* Args must be by copy, otherwise we take a reference to the call stack */
        template <typename FunctionCallType, FunctionCallType Function, typename... Args>
        inline simple_future<int>
        do_op_impl(event_loop::io_handle* _cancle_token, struct io_uring* _ring, Args... _args)
        {
            pause_pack pp = co_await zab::pause(
                [_cancle_token, _ring, _args...](event_loop::io_handle _pp) mutable noexcept
                {
                    if (_cancle_token) { *_cancle_token = _pp; }
                    do_op_impl<FunctionCallType, Function>(
                        _pp,
                        _ring,
                        std::forward<Args>(_args)...);
                });

            co_return pp.data_;
        }

#define do_op(function, ...) do_op_impl<decltype(function), function>(__VA_ARGS__)
    }   // namespace

    event_loop::event_loop()
        : ring_(std::make_unique<io_uring>()), pinned_buffers_(kTotalBuffers * kPinSize),
          use_space_handle_(nullptr)
    {
        struct io_uring_params params;
        ::memset(&params, 0, sizeof(params));
        // params.flags |= IORING_SETUP_SQPOLL;
        // params.sq_thread_idle = 2000;

        int ret = io_uring_queue_init_params(kQueueSize, ring_.get(), &params);
        if (ret < 0)
        {
            std::cerr << "Failed to init io_uring. Return: " << ret << " , errno: " << errno
                      << "\n";
            abort();
        }

        std::vector<struct iovec> ptrs;
        ptrs.reserve(kTotalBuffers);
        for (std::size_t i = 0; i < kTotalBuffers; ++i)
        {
            free_buffers_.emplace_back(
                std::span<std::byte>(pinned_buffers_.data() + i * kPinSize, kPinSize),
                i);
            ptrs.emplace_back(iovec{
                .iov_base = (io_handle) (pinned_buffers_.data() + i * kPinSize),
                .iov_len  = kPinSize});
        }
        io_uring_register_buffers(ring_.get(), ptrs.data(), ptrs.size());

        /* Could also use `EFD_SEMAPHORE`. Not sure what would be best... */
        user_space_event_fd_ = eventfd(0, EFD_CLOEXEC);
        if (user_space_event_fd_ < 0)
        {
            std::cerr << "Failed to init user_space_event_fd_\n";
            abort();
        }
    }

    event_loop::~event_loop()
    {
        for (auto h : handles_[kWriteIndex])
        {
            h.destroy();
        }
        io_uring_queue_exit(ring_.get());

        if (use_space_handle_) { clean_up(use_space_handle_); }
    }

    std::optional<std::pair<std::span<std::byte>, std::size_t>>
    event_loop::claim_fixed_buffer()
    {
        if (free_buffers_.size())
        {
            auto result = free_buffers_.front();
            free_buffers_.pop_front();
            return result;
        }
        else
        {
            return std::nullopt;
        }
    }

    void
    event_loop::release_fixed_buffer(std::pair<std::span<std::byte>, std::size_t> _buffer)
    {
        free_buffers_.emplace_back(_buffer);
    }

    void
    event_loop::submit_pending_events() noexcept
    {
        io_uring_submit(ring_.get());
    }

    simple_future<int>
    event_loop::open_at(
        int                    _dfd,
        const std::string_view _path,
        int                    _flags,
        mode_t                 _mode,
        io_handle*             _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_openat,
            _cancle_token,
            ring_.get(),
            _dfd,
            _path.data(),
            _flags,
            (mode_t) _mode);
    }

    void
    event_loop::open_at(
        io_handle              _cancle_token,
        int                    _dfd,
        const std::string_view _path,
        int                    _flags,
        mode_t                 _mode) noexcept
    {
        return do_op(
            &io_uring_prep_openat,
            _cancle_token,
            ring_.get(),
            _dfd,
            _path.data(),
            _flags,
            (mode_t) _mode);
    }

    simple_future<int>
    event_loop::close(int _fd, io_handle* _cancle_token) noexcept
    {
        return do_op(&io_uring_prep_close, _cancle_token, ring_.get(), _fd);
    }

    void
    event_loop::close(io_handle _cancle_token, int _fd) noexcept
    {
        return do_op(&io_uring_prep_close, _cancle_token, ring_.get(), _fd);
    }

    simple_future<int>
    event_loop::read(
        int                  _fd,
        std::span<std::byte> _buffer,
        off_t                _offset,
        io_handle*           _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_read,
            _cancle_token,
            ring_.get(),
            _fd,
            (char*) _buffer.data(),
            _buffer.size(),
            _offset);
    }

    void
    event_loop::read(
        io_handle            _cancle_token,
        int                  _fd,
        std::span<std::byte> _buffer,
        off_t                _offset) noexcept
    {
        return do_op(
            &io_uring_prep_read,
            _cancle_token,
            ring_.get(),
            _fd,
            (char*) _buffer.data(),
            _buffer.size(),
            _offset);
    }

    simple_future<int>
    event_loop::read_v(
        int                 _fd,
        const struct iovec* _iovecs,
        unsigned            _nr_vecs,
        off_t               _offset,
        io_handle*          _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_readv,
            _cancle_token,
            ring_.get(),
            _fd,
            _iovecs,
            _nr_vecs,
            _offset);
    }

    void
    event_loop::read_v(
        io_handle           _cancle_token,
        int                 _fd,
        const struct iovec* _iovecs,
        unsigned            _nr_vecs,
        off_t               _offset) noexcept
    {
        return do_op(
            &io_uring_prep_readv,
            _cancle_token,
            ring_.get(),
            _fd,
            _iovecs,
            _nr_vecs,
            _offset);
    }

    simple_future<int>
    event_loop::fixed_read(
        int                  _fd,
        std::span<std::byte> _buffer,
        off_t                _offset,
        int                  _buf_index,
        io_handle*           _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_read_fixed,
            _cancle_token,
            ring_.get(),
            _fd,
            (char*) _buffer.data(),
            _buffer.size(),
            _offset,
            _buf_index);
    }

    void
    event_loop::fixed_read(
        io_handle            _cancle_token,
        int                  _fd,
        std::span<std::byte> _buffer,
        off_t                _offset,
        int                  _buf_index) noexcept
    {
        return do_op(
            &io_uring_prep_read_fixed,
            _cancle_token,
            ring_.get(),
            _fd,
            (char*) _buffer.data(),
            _buffer.size(),
            _offset,
            _buf_index);
    }

    simple_future<int>
    event_loop::write(
        int                        _fd,
        std::span<const std::byte> _buffer,
        off_t                      _offset,
        io_handle*                 _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_write,
            _cancle_token,
            ring_.get(),
            _fd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _offset);
    }

    void
    event_loop::write(
        io_handle                  _cancle_token,
        int                        _fd,
        std::span<const std::byte> _buffer,
        off_t                      _offset) noexcept
    {
        return do_op(
            &io_uring_prep_write,
            _cancle_token,
            ring_.get(),
            _fd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _offset);
    }

    simple_future<int>
    event_loop::write_v(
        int                 _fd,
        const struct iovec* _iovecs,
        unsigned            _nr_vecs,
        off_t               _offset,
        io_handle*          _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_writev,
            _cancle_token,
            ring_.get(),
            _fd,
            _iovecs,
            _nr_vecs,
            _offset);
    }

    void
    event_loop::write_v(
        io_handle           _cancle_token,
        int                 _fd,
        const struct iovec* _iovecs,
        unsigned            _nr_vecs,
        off_t               _offset) noexcept
    {
        return do_op(
            &io_uring_prep_writev,
            _cancle_token,
            ring_.get(),
            _fd,
            _iovecs,
            _nr_vecs,
            _offset);
    }

    simple_future<int>
    event_loop::fixed_write(
        int                        _fd,
        std::span<const std::byte> _buffer,
        off_t                      _offset,
        int                        _buf_index,
        io_handle*                 _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_write_fixed,
            _cancle_token,
            ring_.get(),
            _fd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _offset,
            _buf_index);
    }

    void
    event_loop::fixed_write(
        io_handle                  _cancle_token,
        int                        _fd,
        std::span<const std::byte> _buffer,
        off_t                      _offset,
        int                        _buf_index) noexcept
    {
        return do_op(
            &io_uring_prep_write_fixed,
            _cancle_token,
            ring_.get(),
            _fd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _offset,
            _buf_index);
    }

    simple_future<int>
    event_loop::recv(
        int                  sockfd,
        std::span<std::byte> _buffer,
        int                  _flags,
        io_handle*           _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_recv,
            _cancle_token,
            ring_.get(),
            sockfd,
            (char*) _buffer.data(),
            _buffer.size(),
            _flags);
    }

    void
    event_loop::recv(
        io_handle            _cancle_token,
        int                  sockfd,
        std::span<std::byte> _buffer,
        int                  _flags) noexcept
    {
        return do_op(
            &io_uring_prep_recv,
            _cancle_token,
            ring_.get(),
            sockfd,
            (char*) _buffer.data(),
            _buffer.size(),
            _flags);
    }

    simple_future<int>
    event_loop::send(
        int                        sockfd,
        std::span<const std::byte> _buffer,
        int                        _flags,
        io_handle*                 _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_send,
            _cancle_token,
            ring_.get(),
            sockfd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _flags);
    }

    void
    event_loop::send(
        io_handle                  _cancle_token,
        int                        sockfd,
        std::span<const std::byte> _buffer,
        int                        _flags) noexcept
    {
        return do_op(
            &io_uring_prep_send,
            _cancle_token,
            ring_.get(),
            sockfd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _flags);
    }

    simple_future<int>
    event_loop::accept(
        int              _fd,
        struct sockaddr* _addr,
        socklen_t*       _addrlen,
        int              _flags,
        io_handle*       _cancle_token) noexcept
    {
        return do_op(
            &io_uring_prep_accept,
            _cancle_token,
            ring_.get(),
            _fd,
            _addr,
            _addrlen,
            _flags);
    }

    void
    event_loop::accept(
        io_handle        _cancle_token,
        int              _fd,
        struct sockaddr* _addr,
        socklen_t*       _addrlen,
        int              _flags) noexcept
    {
        return do_op(
            &io_uring_prep_accept,
            _cancle_token,
            ring_.get(),
            _fd,
            _addr,
            _addrlen,
            _flags);
    }

    simple_future<int>
    event_loop::connect(
        int              _fd,
        struct sockaddr* _addr,
        socklen_t        _addrlen,
        io_handle*       _cancle_token) noexcept
    {
        return do_op(&io_uring_prep_connect, _cancle_token, ring_.get(), _fd, _addr, _addrlen);
    }

    void
    event_loop::connect(
        io_handle        _cancle_token,
        int              _fd,
        struct sockaddr* _addr,
        socklen_t        _addrlen) noexcept
    {
        return do_op(&io_uring_prep_connect, _cancle_token, ring_.get(), _fd, _addr, _addrlen);
    }

    auto
    event_loop::cancel_event(io_handle _key, bool _resume, std::uintptr_t _cancel_code) noexcept
        -> guaranteed_future<CancelResults>
    {
        auto* sqe = io_uring_get_sqe(ring_.get());

        if (!sqe) { co_return CancelResults::kFailed; }

        sqe->opcode = IORING_OP_ASYNC_CANCEL;
        sqe->addr   = reinterpret_cast<std::uintptr_t>(_key);

        clean_up(_key, _resume, _cancel_code);

        auto pp = co_await zab::pause([&](auto* _pp) noexcept { io_uring_sqe_set_data(sqe, _pp); });

        if (!pp.data_) { co_return CancelResults::kDone; }
        else if (pp.data_ == ENOENT)
        {
            co_return CancelResults::kNotFound;
        }
        else if (pp.data_ == EALREADY)
        {
            co_return CancelResults::kTried;
        }
        else
        {
            co_return CancelResults::kUnknown;
        }
    }

    void
    event_loop::clean_up(io_handle _key, bool _resume, std::uintptr_t _cancel_code) noexcept
    {
        if (!_resume) { _key->handle_.destroy(); }
        else
        {
            _key->data_ = _cancel_code;
            _key->handle_.resume();
        }
    }

    void
    event_loop::wake()
    {
        // TODO: test speed compared to write using io_uring::write
        std::uint64_t item = 1;
        if (::write(user_space_event_fd_, &item, sizeof(item)) < (int) sizeof(item))
        {
            std::cerr << "Notify user space event_fd read failed. Expect halt.\n";
        }
    }

    void
    event_loop::run(std::stop_token _st) noexcept
    {
        run_user_space(_st);

        io_uring_submit(ring_.get());

        static constexpr auto kMaxBatch = 16;
        io_uring_cqe*         completions[kMaxBatch];
        pause_pack*           to_resume[kMaxBatch];

        while (!_st.stop_requested() &&
               !io_uring_wait_cqe(ring_.get(), (io_uring_cqe**) &completions))
        {
            std::uint32_t amount;
            while ((
                amount =
                    io_uring_peek_batch_cqe(ring_.get(), (io_uring_cqe**) &completions, kMaxBatch)))
            {
                /* Pop off the queue... */
                for (std::uint32_t i = 0; i < amount; ++i)
                {
                    to_resume[i] = static_cast<pause_pack*>(io_uring_cqe_get_data(completions[i]));
                    to_resume[i]->data_ = completions[i]->res;
                }

                io_uring_cq_advance(ring_.get(), amount);

                /* Resume them*/
                for (std::uint32_t i = 0; i < amount; ++i)
                {
                    if (to_resume[i]->handle_) { to_resume[i]->handle_.resume(); }
                }

                io_uring_submit(ring_.get());
            }
        }
    }

    void
    event_loop::user_event(event _handle) noexcept
    {
        bool notify = false;
        {
            std::scoped_lock lck(mtx_);
            handles_[kWriteIndex].emplace_back(_handle);
            if (handles_[kWriteIndex].size() == 1) { notify = true; }
            size_.fetch_add(1, std::memory_order_relaxed);
        }

        if (notify) { wake(); }
    }

    async_function<>
    event_loop::run_user_space(std::stop_token _st) noexcept
    {
        std::uint64_t count = 0;
        do
        {
            {
                std::scoped_lock lck(mtx_);
                handles_[kReadIndex].swap(handles_[kWriteIndex]);
                size_.store(0, std::memory_order_relaxed);
            }

            while (handles_[kReadIndex].size())
            {
                handles_[kReadIndex].front().resume();
                handles_[kReadIndex].pop_front();
            }

            auto pp = co_await pause(
                [&](pause_pack* _pp) noexcept
                {
                    use_space_handle_ = _pp;
                    read(
                        _pp,
                        user_space_event_fd_,
                        std::span<std::byte>(
                            static_cast<std::byte*>(static_cast<void*>(&count)),
                            sizeof(count)),
                        0);
                });

            use_space_handle_ = nullptr;

            if (pp.data_ == -1)
            {
                std::cerr << "User space event_fd read failed due to sqe exhaustion.\n";
                break;
            }
            else if (pp.data_ != sizeof(count))
            {
                std::cerr << "User space event_fd read failed.\n";
                break;
            }

        } while (!_st.stop_requested());
    }

}   // namespace zab