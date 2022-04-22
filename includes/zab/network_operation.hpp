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
 *  @file network_operation.hpp
 *
 */

#ifndef ZAB_NETWORK_OPERATION_HPP_
#define ZAB_NETWORK_OPERATION_HPP_

#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdint.h>
#include <sys/socket.h>

#include "zab/async_function.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/pause.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    /**
     * @brief network_operation operation class is a ownership class for
     *        a socket descriptor and a cancelation token.
     *
     */
    class network_operation {

        public:

            /**
             * @brief The value for not having a valid socket descriptor.
             *
             */
            static constexpr int kNoDescriptor = -1;

            using net_op = event_loop::io_event;

            /**
             * @brief Construct a new network operation object in an empty state. Using the
             *        `network_operation` after constructing it with this constructor is
             * undefined behaviour unless `register_engine()` is called with a valid engine.
             *
             */
            network_operation();

            /**
             * @brief Construct a new network operation object and register an engine with it.
             *
             * @param _engine The engine for this object to use.
             */
            network_operation(engine* _engine);

            /**
             * @brief Construct a new network operation object, register an engine with it and take
             *        ownership of the socket descriptor given.
             *
             * @param _engine The engine for this object to use.
             * @param _sd The socket to take ownership of it.
             */
            network_operation(engine* _engine, int _sd);

            /**
             * @brief Destroy the network operation object.
             *
             * @details If `close()` is not awaited before destruction, and the network_operation
             *          holds a valid socket descriptor, its closure is pushed to a background
             *          fibre.
             *
             *          If `cancel()` is not awaited before destruction, and the network_operation
             *          holds a valid io_handle, its cancelation is pushed to a background
             *          fibre.
             *
             */
            ~network_operation();

            /**
             * @brief Cannot copy this object.
             *
             */
            network_operation(const network_operation&) = delete;

            /**
             * @brief Construct a new network operation object moving the resources into it.
             *
             * @param _move The network_operation to move resources from.
             */
            network_operation(network_operation&& _move);

            /**
             * @brief Background close and cancel on the current resources and then take resources
             *        from the given network_operation. See ` ~network_operation();`.
             *
             * @param _move_op The network_operation to move.
             * @return network_operation& this
             */
            network_operation&
            operator=(network_operation&& _move_op);

            /**
             * @brief Swap two network_operation's.
             *
             * @param _first The first network_operation.
             * @param _second The second network_operation.
             */
            friend void
            swap(network_operation& _first, network_operation& _second) noexcept;

            /**
             * @brief Register a new engine with this.
             *
             * @param _engine The engine to register.
             */
            inline void
            register_engine(engine* _engine) noexcept
            {
                engine_ = _engine;
            }

            /**
             * @brief Set the socket descriptor to take ownership of. If this object holds a valid
             *        socket descriptor it is closed in the background.
             *
             * @param _sd The socket descriptor to take owernship of.
             */
            inline void
            set_descriptor(int _sd) noexcept
            {
                if (sd_ >= 0) { background_close(); }
                sd_ = _sd;
            }

            /**
             * @brief Clear the socket descriptor. *Warning* this wipes the state and releases
             *        owernship of the descriptor.
             *
             */
            inline void
            clear_descriptor() noexcept
            {
                sd_ = kNoDescriptor;
            }

            /**
             * @brief Get the engine that is registered.
             *
             * @return engine*
             */
            [[nodiscard]] engine*
            get_engine() noexcept
            {
                return engine_;
            }

            /**
             * @brief Get the underlying socket descriptor.
             *
             * @return int The socket descriptor.
             */
            [[nodiscard]] inline int
            descriptor() const noexcept
            {
                return sd_;
            }

            /**
             * @brief Get the last error that was set. Clears the error.
             *
             * @return int The last error.
             */
            [[nodiscard]] inline int
            last_error() noexcept
            {
                auto tmp    = last_error_;
                last_error_ = 0;
                return tmp;
            }

            /**
             * @brief Get the last error without resetting it.
             *
             * @return int The last error.
             */
            [[nodiscard]] inline int
            peek_error() noexcept
            {
                return last_error_;
            }

            /**
             * @brief Clear the error without checking it.
             *
             */
            inline void
            clear_error() noexcept
            {
                last_error_ = 0;
            }

            /**
             * @brief Set the last error.
             *
             * @param _error The error to set.
             */
            inline void
            set_error(int _error) noexcept
            {
                last_error_ = _error;
            }

            /**
             * @brief Get a reference to the cancelation token held by this object.
             *
             * @return io_handle&
             */
            [[nodiscard]] inline net_op*&
            get_cancel() noexcept
            {
                return cancel_token_;
            }

            /**
             * @brief Set the value cancelaltion token.
             *
             * @param _handle The value to set.
             */
            inline void
            set_cancel(net_op* _handle) noexcept
            {
                cancel_token_ = _handle;
            }

            /**
             * @brief Clear the value of the cancelation token.
             *
             */
            inline void
            clear_cancel() noexcept
            {
                cancel_token_ = nullptr;
            }

            /**
             * @brief Cancel an operation on a given handle.
             *
             * @param _engine The engine to use for cancelation.
             * @param[out] _handle The handle to use and clear on successful cancelation.
             *
             * @co_return void Resumes once the operation has been cancelled or an error occurs.
             */
            [[nodiscard]] static auto
            cancel(engine* _engine, net_op*& _handle) noexcept
            {
                return suspension_point(
                    [_engine, &_handle, ret = net_op{}]<typename T>(T _control) mutable noexcept
                    {
                        if constexpr (is_ready<T>())
                        {
                            /* Do not suspend if nothing to cancel */
                            return !_handle;
                        }
                        else if constexpr (is_suspend<T>())
                        {
                            ret.result_ = -1;
                            ret.handle_ = _control;
                            _engine->get_event_loop().cancel_event(&ret, _handle);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            auto result = event_loop::cancel_code(ret.result_);

                            if (result != event_loop::CancelResult::kFailed) { _handle = nullptr; }

                            return result;
                        }
                    });
            }

            /**
             * @brief Convience function for passing the correct arguments to the static cancel
             *        function.
             *
             * @co_return void Resumes once the operation has been cancelled or an error occurs.
             */
            [[nodiscard]] auto
            cancel() noexcept
            {
                return cancel(engine_, cancel_token_);
            }

            /**
             * @brief Attempt to close the socket.
             *
             * @co_return true If the socket was successfully closed.
             * @co_return false If the socket could not be closed.
             */
            [[nodiscard]] auto
            close() noexcept
            {
                return suspension_point(
                    [this, ret = net_op{}]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_ready<T>())
                        {
                            /* Dont suspend if acceptor_ is invalid. */
                            return sd_ < 0;
                        }
                        else if constexpr (is_suspend<T>())
                        {
                            /* Clear any errors */
                            int       result;
                            socklen_t result_len = sizeof(result);
                            ::getsockopt(sd_, SOL_SOCKET, SO_ERROR, (char*) &result, &result_len);

                            ret.handle_ = _handle;
                            engine_->get_event_loop().close(&ret, sd_);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            if (ret.result_ == 0)
                            {
                                sd_ = kNoDescriptor;
                                return true;
                            }
                            else
                            {
                                last_error_ = -ret.result_;
                                return false;
                            };
                        }
                    });
            }

            /**
             * @brief Attempt to close the socket in the background. Error's in closing cannot be
             *        caught.
             *
             * @return async_function<>
             */
            async_function<>
            background_close() noexcept;

            /**
             * @brief Attempt to cancel the pending operation in the background. Error's in
             *        cancelling cannot be caught.
             *
             * @return async_function<>
             */
            async_function<>
            background_cancel() noexcept;

        private:

            engine* engine_;
            net_op* cancel_token_;
            int     sd_;
            int     last_error_;
    };

}   // namespace zab

#endif /* ZAB_NETWORK_OPERATION_HPP_ */