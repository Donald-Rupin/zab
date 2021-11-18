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
 *  @file reusable_future.hpp
 *
 */

#ifndef ZAB_REUSABLE_FUTURE_HPP_
#define ZAB_REUSABLE_FUTURE_HPP_

#include <coroutine>
#include <optional>

#include "zab/reusable_promise.hpp"
#include "zab/simple_future.hpp"

namespace zab {

    namespace details {

        template <typename Base>
        concept Reoccurring = requires(Base a)
        {
            {
                a.force_completion()
            }
            noexcept->std::same_as<void>;
            {
                a.prepare()
            }
            noexcept->std::same_as<void>;
            {
                a.value_ready()
            }
            noexcept->std::same_as<bool>;
        }
        &&Returnable<Base> && (Returns<Base> || IsVoid<Base>);

    }   // namespace details

    /**
     * @brief      Represents the future value of a reusuable promise.
     *
     * @tparam     T  The type of the promised value.
     */
    template <typename T = void, details::Reoccurring Promise = reusable_promise<T>>
    class reusable_future {

        public:

            /* The promise type */
            using promise_type = Promise;

            /* The handle for the coroutine */
            using coro_handle = std::coroutine_handle<promise_type>;

            /* A type erased handle for the coroutine */
            using erased_coro_handle = std::coroutine_handle<>;

            /* The co_await return value;*/
            using return_value = typename deduce_type<typename Promise::returns>::type;

            /**
             * @brief      Construct with the future with a handle to its coroutine.
             *
             * @param[in]  _coroutine  The coroutine handle.
             */
            reusable_future(coro_handle _coroutine) : handle_(_coroutine) { }

            /**
             * @brief      Destroys the future and cleans up the coroutine handle.
             *
             * @details    We destroy the coroutine handle here as the the final_suspend in
             *             the `reusable_promise` does not resume.
             *
             */
            ~reusable_future()
            {
                /* If we are deconstructing but what we are waiting for isnt complete */
                /* something exceptional must of happened and is causing the coroutine to */
                /* unwind in the wrong direction. This is most likely the engines attempt */
                /* to clean up the event loops on shutdown... */
                if (handle_ && handle_.promise().complete()) { handle_.destroy(); }
            }

            /**
             * @brief      Move Assignment operator.
             *
             * @param      _other  The simple_future to move.
             *
             * @return     *this.
             */
            reusable_future&
            operator=(reusable_future&& _other)
            {
                if (handle_) { handle_.destroy(); }

                handle_ = _other.handle_;

                _other.handle_ = nullptr;

                return *this;
            }

            /**
             * @brief      Cannot be coppied.
             *
             * @param[in]  _other  The reusable_future to copy.
             */
            reusable_future(const reusable_future& _other) = delete;

            /**
             * @brief      Moving makes the moved reusable_future lose ownership of the handle.
             *
             * @param      _other  The reusable_future to move.
             */
            reusable_future(reusable_future&& _other) : handle_(_other.handle_)
            {
                _other.handle_ = nullptr;
            }

            /**
             * @brief      wait for the `reusable_promise` to be fulfilled or fail.
             *
             * @details    Then co_await returns an `std::optional<T>`
             *             which represents if the reusable_promise was fulfilled.
             *
             * @return     A `co_await`'able struct.
             */
            auto operator co_await() const noexcept
            {
                handle_.promise().prepare();
                struct {
                        auto
                        await_suspend(erased_coro_handle _remsumptor) noexcept
                        {
                            handle_.promise().set_underlying(_remsumptor);
                            return handle_;
                        }

                        bool
                        await_ready() const noexcept
                        {
                            return handle_.promise().value_ready();
                        }

                        decltype(auto)
                        await_resume() const noexcept
                        {
                            if constexpr (!std::is_same_v<return_value, promise_void>)
                            {
                                return handle_.promise().data();
                            }
                        }

                        /** The handle of coroutine to execute next. */
                        coro_handle handle_;

                } awaiter{.handle_ = handle_};

                return awaiter;
            }

            /**
             * @brief      Test if the coroutine has fully completed.
             *
             * @return     true if complete, false otherwise.
             */
            inline bool
            complete() const noexcept
            {
                return !handle_ || handle_.promise().complete();
            }

            /**
             * @brief      Test if the coroutine has fully completed.
             *
             * @return     true if complete, false otherwise.
             */
            operator bool() const noexcept { return complete(); }

            /**
             * @brief      Force the end of the coroutine.
             *
             * @details    If this function is called while somthing is `co_await`ing
             *             the corountine, it will cause undefined behaviour.
             */
            inline void
            force_completion() noexcept
            {
                if (handle_)
                {
                    handle_.promise().force_completion();
                    handle_.destroy();
                    handle_ = nullptr;
                }
            }

        private:

            coro_handle handle_;
    };

}   // namespace zab

#endif /* ZAB_REUSABLE_FUTURE_HPP_ */