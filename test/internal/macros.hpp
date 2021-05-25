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
 *  @file macros.hpp
 *
 */

#include <iostream>

#ifndef ZAB_TEST_MACROS_HPP_
#    define ZAB_TEST_MACROS_HPP_

namespace zab::details {
    template <typename T>
    concept Streamable = requires(T value)
    {
        {std::cerr << value};
    };

    template <Streamable T>
    auto&
    Print(const T& _t)
    {
        return std::cerr << _t;
    }

    template <typename T>
    auto&
    Print(const T&)
    {
        return std::cerr << "<Object>";
    }
}   // namespace zab::details

#    define expected(_expected, _got)                                                              \
        [](auto x, auto y)                                                                         \
        {                                                                                          \
            if (x != y)                                                                            \
            {                                                                                      \
                std::cerr << __FILE__ << ":" << __LINE__;                                          \
                std::cerr << ": Expected ";                                                        \
                zab::details::Print(x) << " got ";                                                 \
                zab::details::Print(y) << "\n";                                                    \
                return 1;                                                                          \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                return 0;                                                                          \
            }                                                                                      \
        }(_expected, _got)

#    define not_expected(_expected, _got)                                                           \
        [](auto x, auto y)                                                                         \
        {                                                                                          \
            if (x == y)                                                                            \
            {                                                                                      \
                std::cerr << __FILE__ << ":" << __LINE__;                                          \
                std::cerr << ": Not expected ";                                                    \
                zab::details::Print(x) << " to be ";                                               \
                zab::details::Print(y) << "\n";                                                    \
                return 1;                                                                          \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                return 0;                                                                          \
            }                                                                                      \
        }(_expected, _got)

#endif /* ZAB_TEST_MACROS_HPP_ */