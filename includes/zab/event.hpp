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
 * @TODO: Investigate IOSQE_IO_LINK
 *
 *  @file event.hpp
 *
 */

#ifndef ZAB_EVENT_HPP_
#define ZAB_EVENT_HPP_

#include <coroutine>
#include <cstdint>
#include <functional>
#include <variant>

#include "zab/strong_types.hpp"

namespace zab {

    using event = std::coroutine_handle<>;

    using context_callback = void (*)(void*, int);

    struct io_handle {

            event handle_;

            int result_;
    };

    struct io_context {

            context_callback cb_;

            void* context_;
    };

    struct io_queue {

            event handle_;

            int position_;

            int* results_;
    };

    using io_ptr = void*;

    static constexpr std::uintptr_t kAddressMask = ~(0b11ull);

    static constexpr std::uintptr_t kHandleFlag  = 0b00;
    static constexpr std::uintptr_t kContextFlag = 0b01;
    static constexpr std::uintptr_t kQueueFlag   = 0b10;

    inline void
    execute_io(io_ptr _io_address, int _result) noexcept
    {
        std::uintptr_t io_address = reinterpret_cast<std::uintptr_t>(_io_address) & kAddressMask;
        std::uintptr_t flag       = reinterpret_cast<std::uintptr_t>(_io_address) & ~kAddressMask;
        if (flag == kHandleFlag)
        {
            /* Handles are allowed to be nullptr */
            if (io_address)
            {
                io_handle* handle = reinterpret_cast<io_handle*>(io_address);
                handle->result_   = _result;
                handle->handle_.resume();
            }
        }
        else if (flag == kContextFlag)
        {
            io_context* context = reinterpret_cast<io_context*>(io_address);
            (*context->cb_)(context->context_, _result);
        }
        else
        {
            io_queue* queue                   = reinterpret_cast<io_queue*>(io_address);
            queue->results_[queue->position_] = _result;

            if (queue->position_) { --queue->position_; }
            else
            {
                queue->handle_.resume();
            }
        }
    }

    inline constexpr io_ptr
    create_ptr(io_ptr _ptr, std::uintptr_t _flag) noexcept
    {
        return reinterpret_cast<io_ptr>(reinterpret_cast<std::uintptr_t>(_ptr) | _flag);
    }

}   // namespace zab

#endif /* ZAB_EVENT_HPP_ */