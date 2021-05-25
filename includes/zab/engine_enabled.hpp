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
 *  @file engine_enabled.hpp
 *
 */

#ifndef ZAB_ENGINE_ENABLED_HPP_
#define ZAB_ENGINE_ENABLED_HPP_

#include <compare>
#include <optional>
#include <type_traits>

#include "zab/async_primitives.hpp"
#include "zab/engine.hpp"
#include "zab/event.hpp"
#include "zab/reusable_promise.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"
#include "zab/wait_for.hpp"

namespace zab {

    /** Internal details */
    namespace details {

        /**
         * @brief      Since we cannot enforce
         * std::is_base_of<engine_enabled<Base>, Base> due to circular depandancy
         * we instead have a common ancestor to both Base and engine_enabled
         * (configs -> engine_enabled -> Base).
         *
         * @details    An object could still satisfy std::is_base_of<configs,
         * Base> by directly inheriting from configs instead of engine_enabled.
         *             But if they purposely do this then they can expect things
         * to break :)
         */
        struct configs {
                /* How oftern to trigger the main function */
                static constexpr auto kMainCadence = order::seconds(30);

                /* Which thread to use by default */
                static constexpr auto kDefaultThread = event_loop::kAnyThread;

                /* Which thread to use for initialisation */
                static constexpr auto kInitialiseThread = kDefaultThread;

                /* Which thread to use for the main */
                static constexpr auto kMainThread = kDefaultThread;

                /* Which slot to use for API's */
                static constexpr auto kSlotToUse = std::numeric_limits<size_t>::max();
        };

        /**
         * @brief      Has an `void initialise(void)` function.
         *
         * @tparam     Base  The class to test.
         */
        template <typename Base>
        concept HasInitialise = requires(Base a)
        {
            {
                a.initialise()
            }
            noexcept->std::same_as<void>;
        };

        /**
         * @brief      Has an `void main(void)` function.
         *
         * @tparam     Base  The class to test.
         */
        template <typename Base>
        concept HasMain = requires(Base a)
        {
            {
                a.main()
            }
            noexcept->std::same_as<void>;
        };

    }   // namespace details

    /**
     * @brief      This class describes an engine enabled wrapper for class.
     *
     * @tparam     Base  The class to wrap.
     */
    template <typename Base>
    class engine_enabled : public details::configs {
            friend Base;

            /**
             * @brief      Convert this to its child.
             *
             * @return     `this` as a `Base&`
             */
            inline Base&
            underlying() & noexcept
            {
                return static_cast<Base&>(*this);
            }

            /**
             * @brief      Convert this to its child.
             *
             * @return     `this` as a `const Base&`
             */
            inline const Base&
            underlying() const& noexcept
            {
                return static_cast<const Base&>(*this);
            }

        public:

            /**
             * @brief      Default constructed.
             */
            engine_enabled() = default;

            /**
             * @brief      Default destroyed.
             */
            ~engine_enabled() = default;

            /**
             * @brief      Register this class with an `engine`
             *
             * @details    If the class statisfies `details::HasInitialise<F>`
             *             then `initialise()` will be called on engine start.
             *
             *             If the class satisfies `details::HasMain<F>`
             *             then `main()` will be called on a loop of
             *             cadence `kMainCadence`. The Base class can overwrite
             *             the value of `kMainCadence`.
             *
             *             If the class satisfies `details::HasAPI<F>`
             *             then the `API` returned by `APIInterface()` will
             *             registered against the `engine`s interface. If
             *             the does not provide a `kSlotToUse` then one is
             *             dynamically selected and the class must provide a
             *             `Slot(size_t)` function will report the slot provided.
             *
             *             Usuage of `YieldCode()`, `HitAPI()`, `Respond()` or
             * `Fail()` results in undefined behaviour if an engine has not been
             *             registered.
             *
             * @param      _engine  The engine to register against.
             *
             * @tparam     F        The `Base` class.
             *
             * @return     true if successful, 0 otherwise.
             */
            template <typename F = Base>
            bool
            register_engine(engine& _engine) noexcept
            {
                bool result = true;

                engine_ = &_engine;
                if constexpr (details::HasInitialise<F>)
                {
                    code_block(
                        [this](auto) noexcept { underlying().initialise(); },
                        next(),
                        thread_t(Base::kInitialiseThread));
                }
                if constexpr (details::HasMain<F>) { do_main<F>(); }

                return result;
            }

            engine*
            get_engine() const noexcept
            {
                return engine_;
            }

            friend void
            swap(engine_enabled& _first, engine_enabled& _second) noexcept
            {

                std::swap(_first.engine_, _second.engine_);
            }

        protected:

            /**
             * @brief      Create an order_t.
             *
             * @param[in]  _order  The order to apply.
             *
             * @return     The order_t.
             */
            static constexpr order_t
            order(int64_t _order)
            {
                return {_order};
            }

            /**
             * @brief      Create an order_t set to `order::now()`.
             *
             * @return     The order_t.
             */
            static inline order_t
            now() noexcept
            {
                return {order::now()};
            }

            /**
             * @brief      Create an order_t that specifies to be executed next.
             *
             * @return     The order_t.
             */
            static constexpr order_t
            next()
            {
                return {0};
            }

            /**
             * @brief      Create a thread_t targeting a spercific thread.
             *
             * @param[in]  _thread  The thread to target.
             *
             * @return     The thread_t.
             */
            static constexpr thread_t
            thread(std::uint16_t _thread)
            {
                return {_thread};
            }

            /**
             * @brief      Create a thread_t targeting any thread.
             *
             * @return     The thread_t.
             */
            static constexpr thread_t
            any_thread()
            {
                return {};
            }

            /**
             * @brief      Create a thread_t targeting the default thread.
             *
             * @return     The thread_t.
             */
            static constexpr thread_t
            default_thread()
            {
                return {Base::kDefaultThread};
            }

            /**
             * @brief      Defer some code to be executed at a later time.
             *
             * @param[in]  _cb        The code to execute later.
             * @param[in]  _ordering  The ordering to apply.
             * @param[in]  _thread    The thread to use.
             */
            template <typename Functor>
                requires(std::is_nothrow_invocable_v<Functor, thread_t>)
            inline void
            code_block(
                Functor&&  _cb,
                order_t _ordering = now(),
                thread_t   _thread   = default_thread()) const noexcept
            {
                get_engine()->execute(
                    zab::code_block{.cb_ = std::forward<Functor>(_cb)},
                    _ordering,
                    _thread);
            }

            /**
             * @brief      Responce to a ResponcePack.
             *
             * @param[in]  data_  The data
             * @param[in]  resp_  The pack to respond to.
             */
            [[nodiscard]] inline auto
            yield(order_t _order = now(), thread_t _thread = default_thread()) const noexcept
            {
                return zab::yield(get_engine(), _order, _thread);
            }

            [[nodiscard]] inline auto
            yield(thread_t _thread) const noexcept
            {
                return zab::yield(get_engine(), now(), _thread);
            }



            inline void
            unpause(pause_pack& _pause, order_t _order = now()) const
            {
                zab::unpause(get_engine(), _pause, _order);
            }

            template <typename... Promises>
            [[nodiscard]] inline auto

            wait_for(Promises&&... _args) const
            {
                return zab::wait_for(get_engine(), std::forward<Promises>(_args)...);
            }

            template <typename T>
            [[nodiscard]] inline auto

            wait_for(std::vector<simple_future<T>>&& _args) const
            {
                return zab::wait_for(get_engine(), std::move(_args));
            }

            template <typename Promise, typename... Parameters>
            using async_member = async_function<Promise> (Base::*)(Parameters...);

            template <typename Promise, typename... Parameters>
            using async_member_c = async_function<Promise> (Base::*)(Parameters...) const;

            template <typename Promise, typename Return, typename... Parameters>
            using simple_member = simple_future<Return, Promise> (Base::*)(Parameters...);

            template <typename Promise, typename Return, typename... Parameters>
            using simple_member_c = simple_future<Return, Promise> (Base::*)(Parameters...) const;

            template <typename Promise, typename Return, typename... Parameters>
            using reusable_member = reusable_future<Return, Promise> (Base::*)(Parameters...);

            template <typename Promise, typename Return, typename... Parameters>
            using reusable_member_c =
                reusable_future<Return, Promise> (Base::*)(Parameters...) const;

            template <typename Promise, typename... Args, typename... Parameters>
            async_function<>
            proxy(
                async_member<Promise, Parameters...> _func,
                thread_t                             _required_thread,
                Args... _args)
            {
                thread_t return_thread;

                if (auto t = get_engine()->get_event_loop().current_id();
                    _required_thread != thread_t{} && _required_thread != t)
                {
                    return_thread = t;

                    co_await yield(now(), _required_thread);
                }

                (underlying().*_func)(std::forward<Args>(_args)...);

                if (return_thread != thread_t{}) { co_await yield(now(), return_thread); }
            }

            template <typename Promise, typename... Args, typename... Parameters>
            async_function<>
            proxy(
                async_member_c<Promise, Parameters...> _func,
                thread_t                               _required_thread,
                Args... _args) const
            {
                thread_t return_thread;

                if (auto t = get_engine()->get_event_loop().current_id();
                    _required_thread != thread_t{} && _required_thread != t)
                {
                    return_thread = t;

                    co_await yield(now(), _required_thread);
                }

                (underlying().*_func)(std::forward<Args>(_args)...);

                if (return_thread != thread_t{}) { co_await yield(now(), return_thread); }
            }

            template <typename Promise, typename... Args, typename Return, typename... Parameters>
            simple_future<Return, Promise>
            proxy(
                simple_member<Promise, Return, Parameters...> _func,
                thread_t                                      _required_thread,
                Args... _args)
            {
                auto func = (underlying().*_func)(std::forward<Args>(_args)...);

                thread_t return_thread;

                if (auto t = get_engine()->get_event_loop().current_id();
                    _required_thread != thread_t{} && _required_thread != t)
                {
                    return_thread = t;

                    co_await yield(now(), _required_thread);
                }

                typename simple_future<Return>::return_value result = co_await func;

                if (return_thread != thread_t{}) { co_await yield(now(), return_thread); }

                co_return result;
            }

            template <typename Promise, typename... Args, typename Return, typename... Parameters>
            simple_future<Return>
            proxy(
                simple_member_c<Promise, Return, Parameters...> _func,
                thread_t                                        _required_thread,
                Args... _args) const
            {
                auto func = (underlying().*_func)(std::forward<Args>(_args)...);

                thread_t return_thread;

                if (auto t = get_engine()->get_event_loop().current_id();
                    _required_thread != thread_t{} && _required_thread != t)
                {
                    return_thread = t;

                    co_await yield(now(), _required_thread);
                }

                typename simple_future<Return>::return_value result = co_await func;

                if (return_thread != thread_t{}) { co_await yield(now(), return_thread); }

                co_return result;
            }

            template <typename Promise, typename... Args, typename Return, typename... Parameters>
            reusable_future<Return>
            proxy(
                reusable_member<Promise, Return, Parameters...> _func,
                thread_t                                        _required_thread,
                Args... _args)
            {
                thread_t return_thread = get_engine()->get_event_loop().current_id();

                reusable_future<Return> generator =
                    (underlying().*_func)(std::forward<Args>(_args)...);

                while (true)
                {
                    if (_required_thread != thread_t{} && _required_thread != return_thread)
                    {
                        co_await yield(now(), _required_thread);
                    }

                    typename simple_future<Return>::return_value result = co_await generator;
                    if (return_thread != thread_t{} && _required_thread != return_thread)
                    {
                        co_await yield(now(), return_thread);
                    }
                    if (!generator.complete()) { co_yield result; }
                    else
                    {
                        co_return result;
                    }
                }
            }

            template <typename Promise, typename... Args, typename Return, typename... Parameters>
            reusable_future<Return>
            proxy(
                reusable_member_c<Promise, Return, Parameters...> _func,
                thread_t                                          _required_thread,
                Args... _args)
            {
                thread_t return_thread = get_engine()->get_event_loop().current_id();

                reusable_future<Return> generator =
                    (underlying().*_func)(std::forward<Args>(_args)...);

                while (true)
                {
                    if (_required_thread != thread_t{} && _required_thread != return_thread)
                    {
                        co_await yield(now(), _required_thread);
                    }

                    typename simple_future<Return>::return_value result = co_await generator;
                    if (return_thread != thread_t{} && _required_thread != return_thread)
                    {
                        co_await yield(now(), return_thread);
                    }
                    if (!generator.complete()) { co_yield result; }
                    else
                    {
                        co_return result;
                    }
                }
            }

        private:

            /**
             * @brief      Recursiviely `code_block()` the Base class' `main()`.
             *
             * @tparam     F     The Base class.
             */
            template <details::HasMain F = Base>
            void
            do_main() noexcept
            {
                code_block(
                    [this](auto) noexcept
                    {
                        underlying().main();
                        do_main();
                    },
                    order_t(order::seconds(Base::kMainCadence)),
                    thread_t(Base::kMainThread));
            }

            engine* engine_;
    };

}   // namespace zab

#endif /* ZAB_ENGINE_ENABLED_HPP_ */