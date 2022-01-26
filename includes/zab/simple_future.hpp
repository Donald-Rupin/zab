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
 *  @file simple_future.hpp
 *
 */

#ifndef ZAB_SIMPLE_FUTURE_HPP_
#define ZAB_SIMPLE_FUTURE_HPP_

#include <cassert>
#include <coroutine>
#include <optional>
#include <type_traits>
#include <utility>

#include "zab/simple_promise.hpp"

namespace zab {

    /**
     * @brief      An empty struct for void promises
     */
    struct promise_void {
            auto
            operator<=>(const promise_void&) const = default;
    };

    template <typename T>
    struct deduce_type {
            using type = T;
    };

    template <>
    struct deduce_type<void> {
            using type = promise_void;
    };

    namespace details {

        template <typename Base>
        concept Returns = requires(Base a)
        {
            {
                a.data()
            }
            noexcept;
        };
        // TODO(donald): further constrain the above with something like the below.
        //  Does not like this for some reason...
        //  -> std::is_nothrow_convertible_v<
        //      typename Base::return_value
        //  >;

        template <typename Base>
        concept IsVoid = std::same_as<typename Base::returns, void>;

        template <typename Base>
        concept Returnable = requires(Base a)
        {
            {a.initial_suspend()};
        }
        &&(Returns<Base> || IsVoid<Base>);

        template <typename PromiseType>
        struct simple_awaitable {

                template <typename UnderlyingPromise>
                auto
                await_suspend(std::coroutine_handle<UnderlyingPromise> _remsumptor) noexcept
                {
                    handle_.promise().set_underlying(_remsumptor);
                    return handle_;
                }

                bool
                await_ready() const noexcept
                {
                    return false;
                }

                decltype(auto)
                await_resume() const noexcept
                {
                    return handle_.promise().data();
                }

                /** The handle of coroutine to execute next. */
                std::coroutine_handle<PromiseType> handle_;
        };

    }   // namespace details

    /**
     * @brief      Represents the future value of a simple promise.
     *
     * @tparam     T  The type of the promised value.
     */
    template <typename T = void, details::Returnable Promise = simple_promise<T>>
    class simple_future {

        public:

            /* The promise type */
            using promise_type = Promise;

            /* The co_await return value;*/
            using return_value = typename deduce_type<typename promise_type::returns>::type;

            /**
             * @brief      Construct with the future with a handle to its coroutine.
             *
             * @param[in]  _coroutine  The coroutine handle.
             */
            simple_future(std::coroutine_handle<promise_type> _handle) : handle_(_handle) { }

            /**
             * @brief      Destroys the future and cleans up the coroutine handle.
             *
             * @details    We destroy the coroutine handle here as the the
             * final_suspend in the `simple_promise` does not resume.
             *
             */
            ~simple_future()
            {
                /* If we are deconstructing but what we are waiting for isnt complete */
                /* something exceptional must of happened and is causing the coroutine to */
                /* unwind in the wrong direction. This is most likely the engines attempt */
                /* to clean up the event loops on shutdown... */
                if (handle_)
                {
                    if (handle_.promise().is_complete()) { handle_.destroy(); }
                }
            }

            /**
             * @brief      Cannot be coppied.
             *
             * @param[in]  _other  The simple_future to copy.
             */
            simple_future(const simple_future& _other) = delete;

            /**
             * @brief      Moving makes the moved simple_future lose ownership of the
             * handle.
             *
             * @param      _other  The simple_future to move.
             */
            simple_future(simple_future&& _other) : handle_(std::move(_other.handle_))
            {
                _other.handle_ = nullptr;
            }

            /**
             * @brief      Move Assignment operator.
             *
             * @param      _other  The simple_future to move.
             *
             * @return     *this.
             */
            simple_future&
            operator=(simple_future&& _other)
            {
                if (handle_) { handle_.destroy(); }

                handle_ = _other.handle_;

                _other.handle_ = nullptr;

                return *this;
            }

            /**
             * @brief      wait for the `simple_promise` to be fulfilled or fail.
             *
             * @details    Then co_await returns an `std::optional<T>&&`
             *             which represents if the simple_promise was fulfilled or
             * failed.
             *
             * @return     A `co_await`'able struct.
             */
            inline details::simple_awaitable<promise_type> operator co_await() noexcept
            {
                assert((bool) handle_);
                return details::simple_awaitable<promise_type>{.handle_ = handle_};
            }

            std::coroutine_handle<promise_type> handle_;
    };

    /**
     * Conveince for when the promise will always resolve.
     */
    template <typename T>
    using guaranteed_future =
        simple_future<promise_always_resolves<T>, simple_promise<promise_always_resolves<T>>>;

}   // namespace zab

#endif /* ZAB_SIMPLE_FUTURE_HPP_ */