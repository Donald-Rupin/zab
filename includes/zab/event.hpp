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

// delete
#include <iostream>

namespace zab {

    static constexpr std::uintptr_t kAddressMask = ~static_cast<std::uintptr_t>(0b11);
    static constexpr std::uintptr_t kHandleFlag  = static_cast<std::uintptr_t>(0b00);
    static constexpr std::uintptr_t kContextFlag = static_cast<std::uintptr_t>(0b01);

    inline constexpr void*
    synth_ptr(void* _ptr) noexcept
    {
        return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(_ptr) & kAddressMask);
    }

    inline constexpr bool
    is_handle(void* _ptr) noexcept
    {
        return !(reinterpret_cast<std::uintptr_t>(_ptr) & ~kAddressMask);
    }

    inline constexpr bool
    is_context(void* _ptr) noexcept
    {
        return (bool) (reinterpret_cast<std::uintptr_t>(_ptr) | kContextFlag);
    }

    using event = std::coroutine_handle<>;
    namespace details {
        template <typename ReturnType>
        struct context_generator {
                using context_callback = void (*)(void*, ReturnType);
        };

        template <>
        struct context_generator<void> {
                using context_callback = void (*)(void*);
        };

    }   // namespace details
    template <typename ReturnType>
    using context_callback = details::context_generator<ReturnType>::context_callback;

    template <typename ReturnType>
    struct context {

            context_callback<ReturnType> cb_;

            void* context_;
    };

    struct event_ptr {
            void* p_;

            constexpr event_ptr&
            operator=(std::nullptr_t)
            {
                p_ = nullptr;
                return *this;
            }

            constexpr operator bool() const { return (bool) p_; }
    };

    inline constexpr event_ptr
    create_event_ptr(void* _ptr, std::uintptr_t _flag) noexcept
    {
        return {reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(_ptr) | _flag)};
    }

    inline constexpr event_ptr
    create_event_ptr(event _event) noexcept
    {
        return {reinterpret_cast<void*>(
            reinterpret_cast<std::uintptr_t>(_event.address()) | kHandleFlag)};
    }

    template <typename ReturnType>
    void
    execute_event(event_ptr _event_address, ReturnType _result) noexcept
    {
        std::uintptr_t flag = reinterpret_cast<std::uintptr_t>(_event_address) & ~kAddressMask;

        if (flag == kHandleFlag)
        {
            /* Handles are allowed to be nullptr */
            if (_event_address)
            {
                std::coroutine_handle<>::from_address(_event_address.p_).resume();
            }
        }
        else
        {
            auto* cont = reinterpret_cast<context<ReturnType>*>(synth_ptr(_event_address.p_));
            (*cont->cb_)(cont->context_, _result);
        }
    }

    inline void
    execute_event(event_ptr _event_address) noexcept
    {
        std::uintptr_t flag = reinterpret_cast<std::uintptr_t>(_event_address.p_) & ~kAddressMask;

        if (flag == kHandleFlag)
        {
            /* Handles are allowed to be nullptr */
            if (_event_address)
            {
                std::coroutine_handle<>::from_address(_event_address.p_).resume();
            }
        }
        else
        {
            auto* cont = reinterpret_cast<context<void>*>(synth_ptr(_event_address.p_));
            (*cont->cb_)(cont->context_);
        }
    }

    struct io_handle {

            event handle_;

            int result_;
    };

    using io_context = context<int>;

    struct io_buffer {

            event handle_;

            int* results_;

            int position_;
    };

    struct io_ptr {
            void* p_;

            constexpr io_ptr&
            operator=(std::nullptr_t)
            {
                p_ = nullptr;
                return *this;
            }

            constexpr operator bool() const { return (bool) p_; }
    };

    static constexpr std::uintptr_t kQueueFlag = static_cast<std::uintptr_t>(0b10);

    inline void
    execute_io(void* _io_address, int _result) noexcept
    {
        std::uintptr_t flag = reinterpret_cast<std::uintptr_t>(_io_address) & ~kAddressMask;
        if (flag == kHandleFlag)
        {
            /* Handles are allowed to be nullptr */
            if (_io_address)
            {
                io_handle* handle = static_cast<io_handle*>(_io_address);
                handle->result_   = _result;
                handle->handle_.resume();
            }
        }
        else if (flag == kContextFlag)
        {
            auto* context = static_cast<io_context*>(synth_ptr(_io_address));
            (*context->cb_)(context->context_, _result);
        }
        else
        {
            auto* queue                       = static_cast<io_buffer*>(synth_ptr(_io_address));
            queue->results_[queue->position_] = _result;

            if (queue->position_) { --queue->position_; }
            else
            {
                queue->handle_.resume();
            }
        }
    }

    inline constexpr io_ptr
    create_io_ptr(void* _ptr, std::uintptr_t _flag) noexcept
    {
        return {reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(_ptr) | _flag)};
    }

}   // namespace zab

#endif /* ZAB_EVENT_HPP_ */