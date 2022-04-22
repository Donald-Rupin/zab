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

    template <typename EventType = void>
    struct event {
            details::context_generator<EventType>::context_callback cb_;
            void*                                                   context_;
    };

    using tagged_event = std::variant<event<>, std::coroutine_handle<>>;

    template <typename EventType>
    struct storage_event {
            tagged_event handle_;
            EventType    result_;
    };

    template <>
    struct storage_event<void> {
            tagged_event handle_;
    };

    template <typename ReturnType>
    void
    execute_event(event<ReturnType>* _event_address, ReturnType _result) noexcept
    {
        (*_event_address->cb_)(_event_address->context_, _result);
    }

    inline void
    execute_event(event<>* _event_address) noexcept
    {
        (*_event_address->cb_)(_event_address->context_);
    }

    inline void
    execute_event(event<> _event_address) noexcept
    {
        (*_event_address.cb_)(_event_address.context_);
    }

    inline void
    execute_event(tagged_event* _event_address) noexcept
    {
        std::visit(
            []<typename T>(T _handle)
            {
                if constexpr (std::is_same_v<T, event<>>) { (*_handle.cb_)(_handle.context_); }
                else
                {
                    if (_handle) { _handle.resume(); }
                }
            },
            *_event_address);
    }

    inline void
    execute_event(tagged_event _event_address) noexcept
    {
        std::visit(
            []<typename T>(T _handle)
            {
                if constexpr (std::is_same_v<T, event<>>) { (*_handle.cb_)(_handle.context_); }
                else
                {
                    _handle.resume();
                }
            },
            _event_address);
    }

    template <typename EventType>
    void
    execute_event(storage_event<EventType>* _event_address, EventType _result) noexcept
    {
        _event_address->result_ = _result;
        std::visit(
            []<typename T>(T _handle)
            {
                if constexpr (std::is_same_v<T, event<>>) { (*_handle.cb_)(_handle.context_); }
                else
                {
                    if (_handle) { _handle.resume(); }
                }
            },
            *_event_address);
    }

    template <typename EventType>
    event<EventType>
    create_generic_event(storage_event<EventType>* _event)
    {
        return event<EventType>{
            .cb_ =
                +[](void* _context, EventType _et)
                {
                    auto ce     = static_cast<storage_event<EventType>*>(_context);
                    ce->result_ = _et;
                    execute_event(ce->handle_);
                },
            .context_ = _event,
        };
    }

    inline event<>
    create_generic_event(std::coroutine_handle<> _handle)
    {
        return event<>{
            .cb_ =
                +[](void* _context) { std::coroutine_handle<>::from_address(_context).resume(); },
            .context_ = _handle.address(),
        };
    }

    inline event<>
    get_event(tagged_event _event)
    {
        if (_event.index() == 0) { return std::get<0>(_event); }
        else
        {
            return create_generic_event(std::get<1>(_event));
        }
    }

}   // namespace zab

#endif /* ZAB_EVENT_HPP_ */