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
 *  @file network_operation.cpp
 *
 */

#include "zab/network_operation.hpp"

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

#include "zab/event_loop.hpp"

namespace zab {

    network_operation::network_operation()
        : engine_(nullptr), cancel_token_(nullptr), sd_(kNoDescriptor), last_error_(0)
    { }

    network_operation::network_operation(engine* _engine)
        : engine_(_engine), cancel_token_(nullptr), sd_(kNoDescriptor), last_error_(0)
    { }

    network_operation::network_operation(engine* _engine, int _sd)
        : engine_(_engine), cancel_token_(nullptr), sd_(_sd), last_error_(0)
    { }

    network_operation::~network_operation()
    {
        if (cancel_token_) { background_cancel(); }

        if (sd_ >= 0) { background_close(); }
    }

    network_operation::network_operation(network_operation&& _move) : network_operation()
    {
        swap(*this, _move);
    }

    network_operation&
    network_operation::operator=(network_operation&& _move_op)
    {
        if (this != &_move_op)
        {
            if (cancel_token_) { background_cancel(); }

            if (sd_ >= 0) { background_close(); }

            swap(*this, _move_op);
        }

        return *this;
    }

    void
    swap(network_operation& _first, network_operation& _second) noexcept
    {
        using std::swap;
        swap(_first.engine_, _second.engine_);
        swap(_first.cancel_token_, _second.cancel_token_);
        swap(_first.sd_, _second.sd_);
        swap(_first.last_error_, _second.last_error_);
    }

    async_function<>
    network_operation::background_close() noexcept
    {
        auto* e = engine_;

        if (sd_ != kNoDescriptor)
        {
            /* Clear any errors */
            int       err_result;
            socklen_t result_len = sizeof(err_result);
            ::getsockopt(sd_, SOL_SOCKET, SO_ERROR, (char*) &err_result, &result_len);

            auto sd_tmp = sd_;
            sd_         = kNoDescriptor;

            std::uintptr_t result = -1;
            if (e) { result = co_await e->get_event_loop().close(sd_tmp); }

            if (result && ::close(sd_tmp))
            {
                std::cerr << "zab::network_operation->background_close() failed to close a "
                             "socket. errno: "
                          << errno << "\n";
            }
        }
    }

    async_function<>
    network_operation::background_cancel() noexcept
    {
        auto cancel_token_tmp = cancel_token_;
        cancel_token_         = nullptr;
        co_await engine_->get_event_loop().cancel_event(cancel_token_tmp);
    }

}   // namespace zab
