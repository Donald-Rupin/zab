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
 *  @file simple_promise.hpp
 *
 */

#ifndef ZAB_SIMPLE_PROMISE_HPP_
#define ZAB_SIMPLE_PROMISE_HPP_

#include <coroutine>
#include <iostream>
#include <optional>
#include <utility>

#include "zab/spin_lock.hpp"

namespace zab {

    namespace details {
        struct final_suspension {

                template <typename UnderlyingPromise>
                std::coroutine_handle<>
                await_suspend(std::coroutine_handle<UnderlyingPromise> _us) noexcept
                {
                    auto& self = _us.promise();

                    auto next = self.underlying();
                    self.set_underlying(nullptr);
                    self.complete();

                    if (next) { return next; }
                    else
                    {
                        return std::noop_coroutine();
                    }
                }

                bool
                await_ready() const noexcept
                {
                    return false;
                }

                void
                await_resume() const noexcept
                { }
        };
    }   // namespace details

    template <typename T>
    class simple_common {

        public:

            simple_common() : underlying_(nullptr), complete_(false) { }

            ~simple_common()
            {
                if (underlying_) { underlying_.destroy(); }
            }

            inline auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<T>::from_promise(*static_cast<T*>(this));
            }

            inline void
            set_underlying(std::coroutine_handle<> _under) noexcept
            {
                underlying_ = _under;
            }

            inline std::coroutine_handle<>
            underlying() noexcept
            {
                return underlying_;
            }

            inline void
            complete() noexcept
            {
                complete_ = true;
            }

            inline bool
            is_complete() const noexcept
            {
                return complete_;
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it
             * to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            inline auto
            initial_suspend() const noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying
             * coroutine that co_awaited the `simple_common`.
             *
             * @return     A structure for resuming the underlying coroutine.
             */
            inline auto
            final_suspend() const noexcept
            {
                return details::final_suspension{};
            }

            inline void
            unhandled_exception()
            {
                std::cerr << "Unhandled exception in zab coroutine"
                          << "\n";
                abort();
            }

        private:

            // TODO(donald): could remove  complete_ and use null to signal?
            std::coroutine_handle<> underlying_;
            bool                    complete_;
    };

    /**
     * @brief      The promise of some value used be `simple_future`'s.
     *
     * @tparam     T  The type of the promised value.
     */
    template <typename T = void>
    class simple_promise : public simple_common<simple_promise<T>> {

        public:

            using returns = std::optional<T>;

            simple_promise() = default;

            ~simple_promise() = default;

            inline returns&&
            data() noexcept
            {
                return std::move(data_);
            }

            /**
             * @brief      Creates the promised object from some values.
             *
             * @details    One day they may support `co_return x y z;`
             *
             * @param      _args  The arguments to construct T with.
             *
             * @tparam     Args   The types of the arguments.
             */
            template <typename... Args>
            inline void
            return_value(Args&&... _args) noexcept
            {
                data_.emplace(std::forward<Args>(_args)...);
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param      _move  The optional to move.
             */
            inline void
            return_value(returns&& _move) noexcept
            {
                data_.swap(_move);
            }

            /**
             * @brief      No value is returned
             *
             */
            inline void
            return_value(std::nullopt_t) noexcept
            { }

            /**
             * @brief      Construct the optional of T.
             *
             * @param[in]  _copy  The optional to copy
             */
            inline void
            return_value(const returns& _copy) noexcept
            {
                data_ = _copy;
            }

        private:

            returns data_;
    };

    /**
     * @brief      Instructs the Future that the optional return value
     *             will always resolve. That way the co_await will return
     *             a T&& instead of a std::optional<T>&&.
     *
     *
     * @tparam     T     The return type.
     */
    template <typename T>
    struct promise_always_resolves { };

    template <typename T>
    class simple_promise<promise_always_resolves<T>>
        : public simple_common<simple_promise<promise_always_resolves<T>>> {

        public:

            using returns = T;

            simple_promise() = default;

            ~simple_promise() = default;

            inline returns&&
            data() noexcept
            {
                return std::move(*data_);
            }

            /**
             * @brief      Creates the promised object from some values.
             *
             * @details    One day they may support `co_return x y z;`
             *
             * @param      _args  The arguments to construct T with.
             *
             * @tparam     Args   The types of the arguments.
             */
            template <typename... Args>
            inline void
            return_value(Args&&... _args) noexcept
            {
                data_.emplace(std::forward<Args>(_args)...);
            }

        private:

            std::optional<returns> data_;
    };

    /**
     * @brief      An optimised promise for promising nothing.
     *
     */
    template <>
    class simple_promise<void> : public simple_common<simple_promise<void>> {

        public:

            using returns = void;

            simple_promise() = default;

            ~simple_promise() = default;

            inline returns
            data() noexcept
            {
                return;
            }

            /**
             * @brief      The coroutine has finished.
             */
            inline void
            return_void() noexcept
            { }
    };

    /**
     * @brief      An optimised promise for promising a bool.
     *
     */
    template <>
    class simple_promise<bool> : public simple_common<simple_promise<bool>> {

        public:

            using returns = bool;

            simple_promise() = default;

            ~simple_promise() = default;

            inline returns
            data() noexcept
            {
                return return_value_;
            }

            /**
             * @brief      The coroutine has finished.
             */
            inline void
            return_value(bool _result) noexcept
            {
                return_value_ = _result;
            }

        private:

            bool return_value_ = false;
    };
}   // namespace zab

#endif /* ZAB_SIMPLE_PROMISE_HPP_ */