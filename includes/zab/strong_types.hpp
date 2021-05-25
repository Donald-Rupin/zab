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
 *  @file strong_types.hpp
 *
 */

#ifndef ZAB_STRONG_TYPES_HPP_
#define ZAB_STRONG_TYPES_HPP_

#include <chrono>
#include <concepts>
#include <cstdint>
#include <limits>
#include <ostream>

namespace zab {

    /**
     * @brief      A struct for providing strict typing of thread ids'.
     */
    struct thread_t {
            std::uint16_t thread_ = std::numeric_limits<std::uint16_t>::max();

            constexpr auto
            operator<=>(const thread_t& _other) const = default;

            template <std::integral Intergral>
            constexpr auto
            operator<=>(const Intergral _number) const
            {
                return thread_ <=> _number;
            }
    };

    namespace thread {
        
        constexpr auto
        in(std::uint16_t _thread) noexcept
        {
            return thread_t{_thread};
        }

        constexpr auto
        any() noexcept
        {
            return thread_t{};
        }
    }

    inline std::ostream&
    operator<<(std::ostream& os, const thread_t _thread)
    {
        os << "thread[" << _thread.thread_ << "]";
        return os;
    }

    /**
     * @brief      A struct for providing strict typing for ordering.
     */
    struct order_t {

            int64_t order_ = std::chrono::high_resolution_clock::now().time_since_epoch().count();

            constexpr auto
            operator<=>(const order_t& _other) const = default;

            template <std::integral Intergral>
            constexpr auto
            operator<=>(const Intergral _number) const
            {
                return order_ <=> _number;
            }

            friend constexpr auto
            operator +(order_t _lhs, order_t _rhs) noexcept 
            {
                return order_t{_lhs.order_ + _rhs.order_};
            }

            friend constexpr auto
            operator -(order_t _lhs, order_t _rhs) noexcept 
            {
                return order_t{_lhs.order_ - _rhs.order_};
            }
    };


    namespace order {

        inline constexpr order_t
        seconds(int64_t _number) noexcept
        {
            return order_t{_number * 1000000000};
        }

        inline constexpr order_t
        milli(int64_t _number) noexcept
        {
            return order_t{_number * 1000000};
        }

        inline order_t
        now() noexcept
        {
            return order_t{};
        }

        inline order_t
        in_seconds(int64_t _number) noexcept
        {
            return  now() + order_t{_number * 1000000000};
        }

        inline order_t
        in_milli(int64_t _number) noexcept
        {
            return  now() + order_t{_number * 1000000000};
        }

    }   // namespace order

}   // namespace zab

#endif /* ZAB_STRONG_TYPES_HPP_ */