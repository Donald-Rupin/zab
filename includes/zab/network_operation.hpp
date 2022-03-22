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

    class network_operation {

        public:

            static constexpr int kNoDescriptor = -1;

            network_operation();

            network_operation(engine* _engine);

            network_operation(engine* _engine, int _sd);

            ~network_operation();

            network_operation(const network_operation&) = delete;

            network_operation(network_operation&& _move);

            network_operation&
            operator=(network_operation&& _move_op);

            friend void
            swap(network_operation& _first, network_operation& _second) noexcept;

            inline void
            register_engine(engine* _engine) noexcept
            {
                engine_ = _engine;
            }

            inline void
            set_descriptor(int _sd) noexcept
            {
                if (sd_ >= 0) { background_close(); }
                sd_ = _sd;
            }

            inline void
            clear_descriptor() noexcept
            {
                sd_ = kNoDescriptor;
            }

            [[nodiscard]] engine*
            get_engine() noexcept
            {
                return engine_;
            }

            [[nodiscard]] inline int
            descriptor() const noexcept
            {
                return sd_;
            }

            [[nodiscard]] inline int
            last_error() noexcept
            {
                auto tmp    = last_error_;
                last_error_ = 0;
                return tmp;
            }

            [[nodiscard]] inline int
            peek_error() noexcept
            {
                return last_error_;
            }

            inline void
            clear_error() noexcept
            {
                last_error_ = 0;
            }

            inline void
            set_error(int _error) noexcept
            {
                last_error_ = _error;
            }

            [[nodiscard]] inline event_loop::io_handle&
            get_cancel() noexcept
            {
                return cancel_token_;
            }

            inline void
            set_cancel(event_loop::io_handle _handle) noexcept
            {
                cancel_token_ = _handle;
            }

            inline void
            clear_cancel() noexcept
            {
                cancel_token_ = nullptr;
            }

            [[nodiscard]] static ZAB_ASYNC_RETURN(void) cancel(
                engine*                _engine,
                event_loop::io_handle& _handle,
                bool                   _resume,
                std::intptr_t _return_code = std::numeric_limits<std::intptr_t>::min()) noexcept
            {
                /* All cancel return codes must be negative */
                if (_return_code > 0) { _return_code = -_return_code; }
                else if (!_return_code)
                {
                    _return_code = std::numeric_limits<std::intptr_t>::min();
                }

                return co_awaitable(
                    [_engine, &_handle, ret = pause_pack{}, _resume, _return_code]<typename T>(
                        T _control) mutable noexcept
                    {
                        if constexpr (is_ready<T>())
                        {
                            /* Dont suspend if nothing to cancel */
                            return !_handle;
                        }
                        else if constexpr (is_suspend<T>())
                        {
                            ret.data_   = -1;
                            ret.handle_ = _control;
                            _engine->get_event_loop().cancel_event(&ret, _handle);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            if (_handle)
                            {
                                auto result = event_loop::cancel_code(ret.data_);

                                if (result == event_loop::CancelResult::kDone)
                                {
                                    event_loop::clean_up(_handle, _resume, _return_code);
                                }

                                if (result != event_loop::CancelResult::kFailed)
                                {
                                    _handle = nullptr;
                                }
                            }
                        }
                    });
            }

            [[nodiscard]] ZAB_ASYNC_RETURN(void) cancel(
                bool          _resume,
                std::intptr_t _return_code = std::numeric_limits<std::intptr_t>::min()) noexcept
            {
                return cancel(engine_, cancel_token_, _resume, _return_code);
            }

            [[nodiscard]] ZAB_ASYNC_RETURN(bool) close() noexcept
            {
                return co_awaitable(
                    [this, ret = pause_pack{}]<typename T>(T _handle) mutable noexcept
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
                            if (ret.data_ == 0)
                            {
                                sd_ = kNoDescriptor;
                                return true;
                            }
                            else
                            {
                                last_error_ = -ret.data_;
                                return false;
                            };
                        }
                    });
            }

            async_function<>
            background_close() noexcept
            {
                /* Clear any errors */
                int       err_result;
                socklen_t result_len = sizeof(err_result);
                ::getsockopt(sd_, SOL_SOCKET, SO_ERROR, (char*) &err_result, &result_len);

                auto sd_tmp = sd_;
                sd_         = kNoDescriptor;
                auto result = co_await engine_->get_event_loop().close(sd_tmp);

                if (result && ::close(sd_tmp))
                {
                    std::cerr << "zab::network_operation->background_close() failed to close a "
                                 "socket. errno: "
                              << errno << "\n";
                }
            }

            async_function<>
            background_cancel() noexcept
            {
                auto cancel_token_tmp = cancel_token_;
                cancel_token_         = nullptr;
                co_await engine_->get_event_loop().cancel_event(cancel_token_tmp, false);
            }

        private:

            engine*               engine_;
            event_loop::io_handle cancel_token_;
            int                   sd_;
            int                   last_error_;
    };

}   // namespace zab

#endif /* ZAB_NETWORK_OPERATION_HPP_ */