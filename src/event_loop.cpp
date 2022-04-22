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

// delete
#include "zab/engine.hpp"
namespace zab {

    namespace {

        template <typename FunctionCallType, FunctionCallType Function, typename... Args>
        inline void
        do_op_impl(event_loop::io_event* _cancel_token, struct io_uring* _ring, Args&&... _args)
        {
            auto* sqe = io_uring_get_sqe(_ring);

            if (sqe)
            {
                (*Function)(sqe, std::forward<Args>(_args)...);

                io_uring_sqe_set_data(sqe, _cancel_token);
            }
            else
            {
                _cancel_token->result_ = -ENOMEM;
                execute_event(_cancel_token->handle_);
            }
        }

#define do_op(function, ...) do_op_impl<decltype(function), function>(__VA_ARGS__)
    }   // namespace

    event_loop::event_loop() : ring_(std::make_unique<io_uring>()), use_space_handle_(nullptr)
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
        io_uring_queue_exit(ring_.get());

        if (use_space_handle_)
        {
            /* We only use coroutine handles here */
            std::get<std::coroutine_handle<>>(use_space_handle_->handle_).destroy();
        }
    }

    void
    event_loop::submit_pending_events() noexcept
    {
        io_uring_submit(ring_.get());
    }

    void
    event_loop::open_at(
        io_event*              _cancel_token,
        int                    _dfd,
        const std::string_view _path,
        int                    _flags,
        mode_t                 _mode) noexcept
    {
        return do_op(
            &io_uring_prep_openat,
            _cancel_token,
            ring_.get(),
            _dfd,
            _path.data(),
            _flags,
            (mode_t) _mode);
    }

    void
    event_loop::close(io_event* _cancel_token, int _fd) noexcept
    {
        return do_op(&io_uring_prep_close, _cancel_token, ring_.get(), _fd);
    }

    void
    event_loop::read(
        io_event*            _cancel_token,
        int                  _fd,
        std::span<std::byte> _buffer,
        off_t                _offset) noexcept
    {
        return do_op(
            &io_uring_prep_read,
            _cancel_token,
            ring_.get(),
            _fd,
            (char*) _buffer.data(),
            _buffer.size(),
            _offset);
    }

    void
    event_loop::read_v(
        io_event*           _cancel_token,
        int                 _fd,
        const struct iovec* _iovecs,
        unsigned            _nr_vecs,
        off_t               _offset) noexcept
    {
        return do_op(
            &io_uring_prep_readv,
            _cancel_token,
            ring_.get(),
            _fd,
            _iovecs,
            _nr_vecs,
            _offset);
    }

    void
    event_loop::fixed_read(
        io_event*            _cancel_token,
        int                  _fd,
        std::span<std::byte> _buffer,
        off_t                _offset,
        int                  _buf_index) noexcept
    {
        return do_op(
            &io_uring_prep_read_fixed,
            _cancel_token,
            ring_.get(),
            _fd,
            (char*) _buffer.data(),
            _buffer.size(),
            _offset,
            _buf_index);
    }

    void
    event_loop::write(
        io_event*                  _cancel_token,
        int                        _fd,
        std::span<const std::byte> _buffer,
        off_t                      _offset) noexcept
    {
        return do_op(
            &io_uring_prep_write,
            _cancel_token,
            ring_.get(),
            _fd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _offset);
    }

    void
    event_loop::write_v(
        io_event*           _cancel_token,
        int                 _fd,
        const struct iovec* _iovecs,
        unsigned            _nr_vecs,
        off_t               _offset) noexcept
    {
        return do_op(
            &io_uring_prep_writev,
            _cancel_token,
            ring_.get(),
            _fd,
            _iovecs,
            _nr_vecs,
            _offset);
    }

    void
    event_loop::fixed_write(
        io_event*                  _cancel_token,
        int                        _fd,
        std::span<const std::byte> _buffer,
        off_t                      _offset,
        int                        _buf_index) noexcept
    {
        return do_op(
            &io_uring_prep_write_fixed,
            _cancel_token,
            ring_.get(),
            _fd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _offset,
            _buf_index);
    }

    void
    event_loop::recv(
        io_event*            _cancel_token,
        int                  sockfd,
        std::span<std::byte> _buffer,
        int                  _flags) noexcept
    {
        return do_op(
            &io_uring_prep_recv,
            _cancel_token,
            ring_.get(),
            sockfd,
            (char*) _buffer.data(),
            _buffer.size(),
            _flags);
    }

    void
    event_loop::send(
        io_event*                  _cancel_token,
        int                        sockfd,
        std::span<const std::byte> _buffer,
        int                        _flags) noexcept
    {
        return do_op(
            &io_uring_prep_send,
            _cancel_token,
            ring_.get(),
            sockfd,
            (const char*) _buffer.data(),
            _buffer.size(),
            _flags);
    }

    void
    event_loop::accept(
        io_event*        _cancel_token,
        int              _fd,
        struct sockaddr* _addr,
        socklen_t*       _addrlen,
        int              _flags) noexcept
    {
        return do_op(
            &io_uring_prep_accept,
            _cancel_token,
            ring_.get(),
            _fd,
            _addr,
            _addrlen,
            _flags);
    }

    void
    event_loop::connect(
        io_event*              _cancel_token,
        int                    _fd,
        const struct sockaddr* _addr,
        socklen_t              _addrlen) noexcept
    {
        return do_op(&io_uring_prep_connect, _cancel_token, ring_.get(), _fd, _addr, _addrlen);
    }

    void
    event_loop::cancel_event(io_event* _cancel_token, cancelation_token _key) noexcept
    {
        auto* sqe = io_uring_get_sqe(ring_.get());
        if (!sqe) [[unlikely]]
        {
            _cancel_token->result_ = -ENOMEM;
            execute_event(_cancel_token->handle_);
        }
        else
        {
            io_uring_prep_cancel(sqe, reinterpret_cast<std::uintptr_t>(_key), 0);
            io_uring_sqe_set_data(sqe, _cancel_token);
        }
    }

    auto
    event_loop::cancel_code(std::intptr_t _result) noexcept -> CancelResult
    {
        if (!_result) { return CancelResult::kDone; }
        else if (_result == -ENOENT)
        {
            return CancelResult::kNotFound;
        }
        else if (_result == -EALREADY)
        {
            return CancelResult::kTried;
        }
        else if (_result == -ENOMEM)
        {
            return CancelResult::kFailed;
        }
        else
        {
            return CancelResult::kUnknown;
        }
    }

    static const std::uint64_t item = 1;

    void
    event_loop::wake(event_loop& _from) noexcept
    {
        do_op(
            &io_uring_prep_write,
            (io_event*) nullptr,
            _from.ring_.get(),
            user_space_event_fd_,
            (const char*) &item,
            sizeof(item),
            0);
    }

    void
    event_loop::wake() noexcept
    {
        std::uint64_t item = 1;
        if (::write(user_space_event_fd_, &item, sizeof(item)) < (int) sizeof(item))
        {
            std::cerr << "Notify user space event_fd read failed. Expect halt.\n";
        }
    }

    void
    event_loop::dispatch_user_event(user_event _handle) noexcept
    {
        bool notify = false;
        {
            std::scoped_lock lck(mtx_);
            handles_[kWriteIndex].emplace_back(_handle);
            notify = handles_[kWriteIndex].size() == 1;
            size_.fetch_add(1, std::memory_order_relaxed);
        }

        if (notify) { wake(); }
    }

    void
    event_loop::run(std::stop_token _st) noexcept
    {
        run_user_space(_st);

        io_uring_submit(ring_.get());

        static constexpr auto kMaxBatch = 16;
        io_uring_cqe*         completions[kMaxBatch];
        io_event*             to_resume[kMaxBatch];

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
                    to_resume[i] = static_cast<io_event*>(io_uring_cqe_get_data(completions[i]));
                    to_resume[i]->result_ = completions[i]->res;
                }

                io_uring_cq_advance(ring_.get(), amount);

                /* Resume them*/
                for (std::uint32_t i = 0; i < amount; ++i)
                {
                    execute_event(to_resume[i]->handle_);
                }

                io_uring_submit(ring_.get());
            }
        }
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

            for (auto handle : handles_[kReadIndex])
            {
                execute_event(handle);
            }
            handles_[kReadIndex].clear();

            auto result = co_await read(
                user_space_event_fd_,
                std::span<std::byte>(
                    static_cast<std::byte*>(static_cast<void*>(&count)),
                    sizeof(count)),
                0,
                &use_space_handle_);
            use_space_handle_ = nullptr;

            if (result < 0) [[unlikely]]
            {
                std::cerr << "User space event_fd read failed due to error. errno: " << result
                          << "\n";
                break;
            }
            else if (result != sizeof(count)) [[unlikely]]
            {
                std::cerr << "User space event_fd read failed.\n";
                break;
            }

        } while (!_st.stop_requested());
    }

}   // namespace zab