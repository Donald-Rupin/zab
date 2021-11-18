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
 *  @file reusable_promise.hpp
 *
 */

#ifndef ZAB_REUSABLE_PROMISE_HPP_
#define ZAB_REUSABLE_PROMISE_HPP_

#include <coroutine>
#include <optional>

#include "zab/simple_future.hpp"
#include "zab/simple_promise.hpp"

namespace zab {

    template <typename T>
    class reusable_promise {

        public:

            using returns = std::optional<T>;

            ~reusable_promise()
            {
                if (underlying_) { underlying_.destroy(); }
            }

            bool
            complete() const noexcept
            {
                return complete_;
            }

            bool
            value_ready() const noexcept
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

            void
            prepare() noexcept
            {
                data_.reset();
            }

            void
            force_completion() const noexcept
            { }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<reusable_promise<T>>::from_promise(*this);
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying coroutine that
             *             co_awaited the `reusable_promise`.
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
                complete_ = true;
                data_.emplace(std::forward<Args>(_args)...);
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param      _move  The optional to move.
             */
            void
            return_value(std::optional<T>&& _move) noexcept
            {
                data_.swap(_move);
                complete_ = true;
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param      _move  The optional to move.
             */
            void
            return_value(std::optional<T>& _move) noexcept
            {
                data_.swap(_move);
                complete_ = true;
            }

            /**
             * @brief      yield some value to the underlying coroutine.
             *
             * @param      _args  The arguments to construct T with.
             *
             * @tparam     Args   The types of the arguments.
             *
             * @return     final suspend.
             */
            template <typename... Args>
            auto
            yield_value(Args&&... _args) noexcept
            {
                data_.emplace(std::forward<Args>(_args)...);

                return final_suspend();
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param      _move  The optional to move.
             */
            auto
            yield_value(std::optional<T>&& _move) noexcept
            {
                data_.swap(_move);

                return final_suspend();
            }

            /**
             * @brief      Construct the optional of T.
             *
             * @param      _move  The optional to move.
             */
            auto
            yield_value(std::optional<T>& _move) noexcept
            {
                data_.swap(_move);

                return final_suspend();
            }

            /**
             * @brief      Returns a nullopt.
             *
             * @param[in]  <unnamed>  Unused.
             */
            void
            return_value(std::nullopt_t) noexcept
            {
                complete_ = true;
            }

            /**
             * @brief      yield a nullopt.
             *
             * @param[in]  <unnamed>  Unused.
             */
            auto
            yield_value(std::nullopt_t) noexcept
            {
                return final_suspend();
            }

            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

            std::coroutine_handle<> underlying_;
            returns                 data_;
            bool                    complete_ = false;
    };

    template <>
    class reusable_promise<void> {

        public:

            using returns = void;

            bool
            complete() const noexcept
            {
                return complete_;
            }

            bool
            value_ready() const noexcept
            {
                return complete_;
            }

            void
            data() noexcept
            { }

            void
            set_underlying(std::coroutine_handle<> _under) noexcept
            {

                underlying_ = _under;
            }

            void
            prepare() noexcept
            { }

            void
            force_completion() const noexcept
            { }

            /**
             * @brief      Gets the coroutine handle from `this`.
             *
             * @return     The coroutine handle.
             */
            auto
            get_return_object() noexcept
            {
                return std::coroutine_handle<reusable_promise<void>>::from_promise(*this);
            }

            /**
             * @brief      Always suspend execution of the promise. wait for it to be co_awaited.
             *
             * @return     A `std::suspend_always`
             */
            auto
            initial_suspend() noexcept
            {
                return std::suspend_always{};
            }

            /**
             * @brief      Final suspension of Promise resumes the underlying coroutine that
             *             co_awaited the `reusable_promise`.
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
             * @brief      Returns a nullopt.
             *
             * @param[in]  <unnamed>  Unused.
             */
            void
            return_void() noexcept
            {
                complete_ = true;
            }

            /**
             * @brief      yield a nullopt.
             *
             * @param[in]  <unnamed>  Unused.
             */
            auto
            yield_value(promise_void) noexcept
            {
                return final_suspend();
            }

            void
            unhandled_exception()
            {
                // TODO(donald) abort?
            }

            std::coroutine_handle<> underlying_;
            bool                    complete_ = false;
    };

}   // namespace zab

#endif /* ZAB_REUSABLE_PROMISE_HPP_ */