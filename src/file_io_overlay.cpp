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
 *  @file file_io_overlay.cpp
 *
 */

#include "zab/file_io_overlay.hpp"

#include <cstdint>
#include <iostream>
#include <sys/stat.h>

#include "zab/async_primitives.hpp"
#include "zab/engine.hpp"

namespace zab {

    async_file::async_file(engine* _engine, const std::string& _path, Options _options)
        : engine_(_engine)
    {
        file_ = ::fopen(_path.c_str(), fopen_options(_options));
    }

    async_file::~async_file() { ::fclose(file_); }

    simple_future<std::vector<char>>
    async_file::read_file(thread_t _return_into) noexcept
    {
        /* Swap into the io thread. */
        if (engine_->get_event_loop().current_id() != io_thread())
        {
            co_await yield(engine_, order_t{order::now()}, io_thread());
        }

        std::vector<char> data;

        struct stat st;
        if (::fstat(::fileno(file_), &st)) { co_return std::nullopt; }

        data.resize(st.st_size);

        bool   complete = 0;
        size_t total    = 0;
        do
        {

            size_t amount_to =
                data.size() - total < kBufferSize ? data.size() - total : kBufferSize;

            size_t amount_read = ::fread(data.data() + total, 1, amount_to, file_);

            if (::ferror(file_) != 0) { co_return std::nullopt; }

            total += amount_read;

            complete = ::feof(file_) ? true : false;

            if (!complete) { co_await yield(engine_, order_t{order::now()}, io_thread()); }

        } while (!complete && total != data.size());

        data.resize(total);

        /* Ensure we re-enter into the correct event. */
        if (_return_into != event_loop::kAnyThread && io_thread() != _return_into)
        {
            co_await yield(engine_, order_t{order::now()}, _return_into);
        }

        co_return data;
    }

    simple_future<std::vector<char>>
    async_file::read_some(thread_t _return_into, size_t _amount) noexcept
    {
        /* Swap into the io thread. */
        if (engine_->get_event_loop().current_id() != io_thread())
        {
            co_await yield(engine_, order_t{order::now()}, io_thread());
        }

        std::vector<char> data;
        data.resize(_amount);

        bool   complete = 0;
        size_t total    = 0;
        do
        {

            size_t amount_to =
                data.size() - total < kBufferSize ? data.size() - total : kBufferSize;

            size_t amount_read = ::fread(data.data() + total, 1, amount_to, file_);

            if (::ferror(file_) != 0) { co_return std::nullopt; }

            total += amount_read;

            complete = ::feof(file_) ? true : false;

            if (!complete) { co_await yield(engine_, order_t{order::now()}, io_thread()); }

        } while (!complete);

        data.resize(total);

        /* Ensure we re-enter into the correct thread. */
        if (_return_into != event_loop::kAnyThread && io_thread() != _return_into)
        {
            co_await yield(engine_, order_t{order::now()}, _return_into);
        }

        co_return data;
    }

    simple_future<bool>
    async_file::write_to_file(thread_t _return_into, std::span<char> _data) noexcept
    {
        /* Swap into the io thread. */
        if (engine_->get_event_loop().current_id() != io_thread())
        {
            co_await yield(engine_, order_t{order::now()}, io_thread());
        }

        size_t position = 0;
        while (position < _data.size())
        {

            size_t amount =
                (_data.size() - position) < kBufferSize ? (_data.size() - position) : kBufferSize;
            auto rc = ::fwrite(_data.data(), 1, amount, file_);

            if (rc != amount && ::ferror(file_) != 0) { co_return false; }

            position += amount;

            if (position < _data.size())
            {
                co_await yield(engine_, order_t{order::now()}, io_thread());
            }
        }

        /* Ensure we re-enter into the correct thread. */
        if (_return_into != event_loop::kAnyThread && io_thread() != _return_into)
        {
            co_await yield(engine_, order_t{order::now()}, _return_into);
        }

        co_return true;
    }

    simple_future<bool>
    async_file::write_to_file(thread_t _return_into, std::vector<char>&& _data) noexcept
    {
        std::vector<char> to_write;
        to_write.swap(_data);

        /* Swap into the io thread. */
        if (engine_->get_event_loop().current_id() != io_thread())
        {
            co_await yield(engine_, order_t{order::now()}, io_thread());
        }

        size_t position = 0;
        while (position < to_write.size())
        {

            size_t amount = (to_write.size() - position) < kBufferSize
                                ? (to_write.size() - position)
                                : kBufferSize;
            auto   rc     = ::fwrite(to_write.data(), 1, amount, file_);

            if (rc != amount && ::ferror(file_) != 0) { co_return false; }

            position += amount;

            if (position < to_write.size())
            {
                co_await yield(engine_, order_t{order::now()}, io_thread());
            }
        }

        /* Ensure we re-enter into the correct thread. */
        if (_return_into != event_loop::kAnyThread && io_thread() != _return_into)
        {
            co_await yield(engine_, order_t{order::now()}, _return_into);
        }

        co_return true;
    }

    thread_t
    async_file::io_thread() noexcept
    {
        std::uint16_t threads = engine_->get_event_loop().number_of_workers() - 1u;
        return thread_t{threads};
    }

    bool
    async_file::position(size_t _pos, Offset _whence) noexcept
    {
        return fseek(file_, _pos, static_cast<int>(_whence)) == 0;
    }

    bool
    async_file::good() noexcept
    {
        return (bool) file_;
    }

}   // namespace zab