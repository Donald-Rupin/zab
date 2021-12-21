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

#include "zab/async_function.hpp"
#include "zab/async_mutex.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event_loop.hpp"
#include "zab/file_io_overlay.hpp"
#include "zab/for_each.hpp"
#include "zab/network_overlay.hpp"
#include "zab/strong_types.hpp"
#include "zab/tcp_stream.hpp"

namespace zab_example {

    class echo_server : public zab::engine_enabled<echo_server> {

        public:

            static constexpr std::uint16_t kDefaultThread = 0;

            echo_server(zab::engine* _e, std::uint16_t _port) : acceptor_(_e), port_(_port)
            {
                register_engine(*_e);
            }

            void
            initialise() noexcept
            {
                run_acceptor();

                /* Since we will run in an infinite loop use ctr-c to cancel program */
                engine_->get_signal_handler().handle(
                    SIGINT,
                    zab::thread_t{kDefaultThread},
                    [this](int) { engine_->stop(); }

                );
            }

            zab::async_function<>
            run_acceptor()
            {
                int connection_count = 0;
                if (acceptor_.listen(AF_INET, port_, 10))
                {
                    std::optional<zab::tcp_stream> stream;
                    while (stream = co_await acceptor_.accept())
                    {
                        run_stream(connection_count++, std::move(*stream));
                        stream.reset();
                    }
                }
                else
                {
                    engine_->stop();
                }
            }

            zab::async_function<>
            run_stream(int _connection_count, zab::tcp_stream _stream)
            {
                zab::thread_t thread{(std::uint16_t)(
                    _connection_count % engine_->get_event_loop().number_of_workers())};
                /* Lets load balance connections between available threads... */
                co_await yield(thread);

                while (!_stream.last_error())
                {
                    auto data = co_await _stream.read();

                    if (data) { co_await _stream.write(*data); }
                    else
                    {

                        break;
                    }
                }

                /* Wait for the stream to shutdown */
                co_await _stream.shutdown();
            }

        private:

            zab::tcp_acceptor acceptor_;

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

    zab::engine e(zab::event_loop::configs{});

    zab_example::echo_server es(&e, port);

    e.start();

    return 0;
}