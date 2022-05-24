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
 *  @file echo_server.cpp
 *
 */

#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <sys/socket.h>
#include <sys/types.h>

#include "zab/async_file.hpp"
#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/for_each.hpp"
#include "zab/strong_types.hpp"
#include "zab/tcp_networking.hpp"
#include "zab/tcp_stream.hpp"
namespace zab_example {

    class echo_server : public zab::engine_enabled<echo_server> {

        public:

            static constexpr std::uint16_t kDefaultThread = 0;
            static constexpr std::uint16_t kPrintThread   = kDefaultThread;

            echo_server(zab::engine* _e, std::uint16_t _port)
                : acceptor_(_e), active_streams_mtx_(_e), port_(_port)
            {
                register_engine(*_e);
            }

            void
            initialise() noexcept
            {
                run_acceptor();

                /* Since we will run in an infinite loop use ctr-c to cleanly cancel program */
                engine_->get_signal_handler().handle(
                    SIGINT,
                    zab::thread_t{kPrintThread},
                    [this](int) { wake_connections(); }

                );
            }

            zab::async_function<>
            wake_connections()
            {
                std::cout << "Waking all connection"
                          << "\n";

                {
                    auto lck = co_await active_streams_mtx_;

                    for (const auto i : active_streams_)
                    {
                        co_await i->shutdown();
                        co_await i->close();
                    }
                }

                std::cout << "Stopping Engine"
                          << "\n";
                engine_->stop();
            }

            zab::async_function<>
            run_acceptor()
            {
                struct sockaddr_storage _address;
                socklen_t               _length = sizeof(_address);

                int connection_count = 0;
                if (acceptor_.listen(AF_INET, port_, 10))
                {
                    std::cout << "Starting acceptor on port " << port_ << "\n";

                    while (true)
                    {
                        auto stream =
                            co_await acceptor_.accept((struct sockaddr*) &_address, &_length);

                        if (stream) { run_stream(connection_count++, std::move(*stream)); }
                        else
                        {
                            break;
                        }
                    }

                    std::cout << "Stopping acceptor with errno " << acceptor_.last_error() << "\n";
                }
                else
                {

                    std::cout << "Failed to start acceptor with errno " << acceptor_.last_error()
                              << "\n";
                    engine_->stop();
                }
            }

            zab::async_function<>
            run_stream(int _connection_count, zab::tcp_stream<std::byte> _stream)
            {
                zab::thread_t thread{
                    (std::uint16_t)(_connection_count % engine_->number_of_workers())};
                /* Lets load balance connections between available threads... */
                co_await yield(thread);

                { /* Insert stream so we can wake it up... */
                    auto lck = co_await active_streams_mtx_;
                    active_streams_.emplace(&_stream);
                }

                print(thread, _connection_count, "Got connection.");

                /* Log received data to file. */
                zab::async_file<std::byte> log_file(engine_);

                auto file_open = co_await log_file.open(
                    "./connection_log." + std::to_string(_connection_count) + ".txt",
                    zab::file::Option::kTrunc);

                std::vector<std::byte> data(1028 * 1028);
                while (!_stream.last_error())
                {
                    auto amount = co_await _stream.read_some(data);

                    if (amount > 0)
                    {
                        print(thread, _connection_count, "Read ", amount, " bytes.");

                        /* echo the data back */
                        co_await _stream.write({data.data(), (std::size_t) amount});

                        /* log data to file */
                        if (file_open)
                        {
                            auto s = co_await log_file.write_to_file(
                                {data.data(), (std::size_t) amount});
                            if (!s) { print(thread, _connection_count, "Failed to log to file."); }
                        }
                    }
                    else
                    {

                        break;
                    }
                }

                print(thread, _connection_count, "Shutting down connection.");

                /* Wait for the stream to shutdown */
                co_await _stream.shutdown();

                /* Remove stream... */
                auto lck = co_await active_streams_mtx_;
                active_streams_.erase(&_stream);
            }

            template <typename... Args>
            zab::async_function<>
            print(zab::thread_t _thread, int _connection_count, Args... _message)
            {
                /* We are always going to cout from the same thread to prevent clobbering... */
                co_await yield(zab::thread_t{kPrintThread});

                std::cout << _thread << ", Connection[" << _connection_count << "]: ";

                inner_print(_message...);

                std::cout << "\n";
            }

            template <typename... Args>
            void
            inner_print(Args&... _message)
            {
                (std::cout << ... << _message);
            }

        private:

            zab::tcp_acceptor acceptor_;

            zab::async_mutex                      active_streams_mtx_;
            std::set<zab::tcp_stream<std::byte>*> active_streams_;

            std::uint16_t port_;
    };

}   // namespace zab_example

int
main(int _argc, const char** _argv)
{
    if (_argc != 2)
    {
        std::cout << "Please enter a port to listen on."
                  << "\n";
        return 1;
    }

    auto port = std::stoi(_argv[1]);

    zab::engine e(zab::engine::configs{});

    zab_example::echo_server es(&e, port);

    e.start();

    return 0;
}