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

namespace zab {

    /**
     * @brief      The promise of some value used be `simple_future`'s.
     *
     * @tparam     T  The type of the promised value.
     */
    template <typename T = void>
    class simple_promise {

        public:

            using returns = std::optional<T>;

            simple_promise() : underlying_(nullptr) { }

            ~simple_promise()
            {
                if (underlying_)
                {
                    underlying_.destroy();
                }
            }

            bool
            complete() const noexcept
            {
                return complete_;
            }

            returns&&
            data() noexcept
            {
                return std::move(data_);
            }

            void
            set_underlying(std::coroutine_handle<> _under) noexcept
            {
                underlying_ = _under;
            }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<simple_promise<T>>::from_promise(*this);
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it
             * to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying
             * coroutine that co_awaited the `simple_promise`.
             *
             * @return     A structure for resuming the underlying coroutine.
             */
            auto
            final_suspend() noexcept
            {
                struct {

                        std::coroutine_handle<>
                        await_suspend(std::coroutine_handle<>) noexcept
                        {
                            if (next_) { return next_; }
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

                        std::coroutine_handle<> next_;

                } final{.next_ = underlying_};

                underlying_ = nullptr;

                return final;
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
            void
            return_value(Args&&... _args) noexcept
            {
                data_.emplace(std::forward<Args>(_args)...);
                complete_ = true;
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param      _move  The optional to move.
             */
            void
            return_value(returns&& _move) noexcept
            {
                data_.swap(_move);
                complete_ = true;
            }

            /**
             * @brief      No value is returned
             *
             */
            void
            return_value(std::nullopt_t) noexcept
            {
                complete_ = true;
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param[in]  _copy  The optional to copy
             */
            void
            return_value(const returns& _copy) noexcept
            {
                data_     = _copy;
                complete_ = true;
            }

            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

        private:

            std::coroutine_handle<> underlying_;
            returns                 data_;
            bool                    complete_ = false;
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
    class simple_promise<promise_always_resolves<T>> {

        public:

            using returns = T;

            ~simple_promise()
            {
                if (underlying_)
                {
                    underlying_.destroy();
                }
            }

            bool
            complete() const noexcept
            {
                return (bool) data_;
            }

            returns&&
            data() noexcept
            {
                return std::move(*data_);
            }

            void
            set_underlying(std::coroutine_handle<> _under) noexcept
            {
                underlying_ = _under;
            }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            std::coroutine_handle<simple_promise<promise_always_resolves<T>>>
            get_return_object() noexcept
            {
                return std::coroutine_handle<
                    simple_promise<promise_always_resolves<T>>>::from_promise(*this);
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it
             * to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying
             * coroutine that co_awaited the `simple_promise`.
             *
             * @return     A structure for resuming the underlying coroutine.
             */
            auto
            final_suspend() noexcept
            {
                struct {
                        std::coroutine_handle<>
                        await_suspend(std::coroutine_handle<>) noexcept
                        {
                            if (next_) { return next_; }
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

                        std::coroutine_handle<> next_;

                } final{.next_ = underlying_};

                underlying_ = nullptr;

                return final;
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
            void
            return_value(Args&&... _args) noexcept
            {
                data_.emplace(std::forward<Args>(_args)...);
            }

            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

        private:

            std::coroutine_handle<> underlying_;
            std::optional<returns>  data_;
    };

    /**
     * @brief      An optimised promise for promising nothing.
     *
     */
    template <>
    class simple_promise<void> {

        public:

            ~simple_promise()
            {
                if (underlying_)
                {
                    underlying_.destroy();
                }
            }

            using returns = void;

            bool
            complete() const noexcept
            {
                return (bool) complete_;
            }

            void
            set_underlying(std::coroutine_handle<> _under) noexcept
            {
                underlying_ = _under;
            }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            std::coroutine_handle<simple_promise<void>>
            get_return_object() noexcept
            {
                return std::coroutine_handle<simple_promise<void>>::from_promise(*this);
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it
             * to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying
             * coroutine that co_awaited the `simple_promise`.
             *
             * @return     A structure for resuming the underlying coroutine.
             */
            auto
            final_suspend() noexcept
            {
                struct {

                        std::coroutine_handle<>
                        await_suspend(std::coroutine_handle<>) noexcept
                        {
                            if (next_) { return next_; }
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

                        std::coroutine_handle<> next_;

                } final{.next_ = underlying_};

                underlying_ = nullptr;

                return final;
            }

            /**
             * @brief      The coroutine has finished.
             */
            void
            return_void() noexcept
            {
                complete_ = true;
            }

            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

            bool                    complete_ = false;
            std::coroutine_handle<> underlying_;
    };

    /**
     * @brief      An optimised promise for promising a bool.
     *
     */
    template <>
    class simple_promise<bool> {

        public:

            using returns = bool;

            ~simple_promise()
            {
                if (underlying_)
                {
                    underlying_.destroy();
                }
            }

            bool
            complete() const noexcept
            {
                return complete_;
            }

            returns
            data() noexcept
            {
                return return_value_;
            }

            void
            set_underlying(std::coroutine_handle<> _under) noexcept
            {
                underlying_ = _under;
            }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            std::coroutine_handle<simple_promise<bool>>
            get_return_object() noexcept
            {
                return std::coroutine_handle<simple_promise<bool>>::from_promise(*this);
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it
             * to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying
             * coroutine that co_awaited the `simple_promise`.
             *
             * @return     A structure for resuming the underlying coroutine.
             */
            auto
            final_suspend() noexcept
            {
                struct {

                        std::coroutine_handle<>
                        await_suspend(std::coroutine_handle<>) noexcept
                        {
                            if (next_) { return next_; }
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

                        std::coroutine_handle<> next_;

                } final{.next_ = underlying_};

                underlying_ = nullptr;

                return final;
            }

            /**
             * @brief      The coroutine has finished.
             */
            void
            return_value(bool _result) noexcept
            {
                complete_     = true;
                return_value_ = _result;
            }

            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

            bool                    complete_     = false;
            bool                    return_value_ = false;
            std::coroutine_handle<> underlying_;
    };
}   // namespace zab

#endif /* ZAB_SIMPLE_PROMISE_HPP_ */