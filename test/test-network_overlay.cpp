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
 *  MIT License
 *
 *  Copyright (c) 2021 Donald-Rupin
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 *  @file test-network_overlay.cpp
 *
 */

#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/network_overlay.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_simple();

    int
    test_stress();

    int
    run_test()
    {
        return test_simple() || test_stress();
    }

    class test_simple_class : public engine_enabled<test_simple_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr auto kBuffer = "01234";

            test_simple_class() = default;

            void
            initialise() noexcept
            {
                acceptor_.register_engine(*get_engine());
                connector_.register_engine(*get_engine());

                run_acceptor();
                run_connector();
            }

            async_function<>
            run_acceptor()
            {
                if (acceptor_.listen(AF_INET, 6998, 10))
                {
                    auto stream_opt = co_await acceptor_.accept();

                    if (stream_opt)
                    {
                        auto amount = co_await stream_opt->write(
                            std::span<const char>(kBuffer, ::strlen(kBuffer)));

                        if (expected(strlen(kBuffer), amount)) { get_engine()->stop(); }

                        co_await stream_opt->shutdown();
                    }
                    else
                    {
                        get_engine()->stop();
                    }
                }
                else
                {

                    get_engine()->stop();
                }
            }

            async_function<>
            run_connector()
            {

                struct addrinfo  hints;
                struct addrinfo* addr;

                memset(&hints, 0, sizeof(hints));
                hints.ai_family   = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_flags    = 0;
                hints.ai_protocol = 0;

                auto success = getaddrinfo("127.0.0.1", "6998", &hints, &addr);

                if (success || !addr)
                {
                    get_engine()->stop();
                    co_return;
                }

                struct addrinfo* tmp = addr;
                while (tmp)
                {
                    auto stream_opt = co_await connector_.connect(
                        (struct sockaddr_storage*) tmp->ai_addr,
                        (sizeof(*tmp->ai_addr)));

                    if (stream_opt)
                    {
                        auto buffer = co_await stream_opt->read(5);

                        if (buffer)
                        {
                            if (!expected(strlen(kBuffer), buffer->size()))
                            {

                                std::string_view og(kBuffer);
                                buffer->emplace_back(0);
                                std::string_view ng(buffer->data());

                                if (!expected(og, ng)) { failed_ = false; }
                            }
                        }

                        co_await stream_opt->shutdown();
                    }

                    tmp = tmp->ai_next;
                }

                freeaddrinfo(addr);
                get_engine()->stop();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            tcp_acceptor acceptor_;

            tcp_connector connector_;

            bool failed_ = true;
    };

    int
    test_simple()
    {

        engine engine(event_loop::configs{4});

        test_simple_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_stress_class : public engine_enabled<test_stress_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr auto kBuffer = "01234";

            static constexpr std::uint16_t kPorts[]             = {7000, 7001};
            static constexpr size_t        kNumberOfConnections = 500;
            static constexpr size_t        kDataToSend          = 1028 * 257;
            static const std::vector<char> kData;

            test_stress_class() : connections_(0) { }

            void
            initialise() noexcept
            {
                run_acceptor(kPorts[0]);
                run_acceptor(kPorts[1]);

                for (size_t i = 0; i < kNumberOfConnections; ++i)
                {
                    run_connector(kPorts[i & 0b1]);
                }
            }

            async_function<>
            run_acceptor(std::uint16_t _port)
            {
                tcp_acceptor acceptor(get_engine());

                if (acceptor.listen(AF_INET, _port, kNumberOfConnections / 2))
                {
                    while (true)
                    {
                        auto stream_opt = co_await acceptor.accept();
                        if (stream_opt) { run_stream(std::move(*stream_opt)); }
                        else
                        {
                            get_engine()->stop();
                            break;
                        }
                    }
                }
                else
                {

                    get_engine()->stop();
                }
            }

            async_function<>
            run_connector(std::uint16_t _port)
            {
                tcp_connector connector(get_engine());

                struct addrinfo  hints;
                struct addrinfo* addr;

                memset(&hints, 0, sizeof(hints));
                hints.ai_family   = AF_UNSPEC;
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_flags    = 0;
                hints.ai_protocol = 0;

                auto success =
                    getaddrinfo("127.0.0.1", std::to_string(_port).c_str(), &hints, &addr);

                if (success || !addr)
                {
                    get_engine()->stop();
                    co_return;
                }

                struct addrinfo* tmp = addr;
                while (tmp)
                {
                    auto stream_opt = co_await connector.connect(
                        (struct sockaddr_storage*) tmp->ai_addr,
                        (sizeof(*tmp->ai_addr)));

                    if (stream_opt)
                    {
                        run_stream(std::move(*stream_opt));

                        break;
                    }

                    tmp = tmp->ai_next;
                }

                freeaddrinfo(addr);
            }

            async_function<>
            run_stream(tcp_stream _stream)
            {
                _stream.write_and_forget(std::vector<char>(kData));

                auto stream_opt = co_await _stream.read(kDataToSend);

                if (!stream_opt || *stream_opt != kData)
                {
                    engine_->stop();
                    co_return;
                }

                auto cons = connections_.fetch_add(1);

                co_await _stream.shutdown();

                if (cons == (2 * kNumberOfConnections) - 1)
                {
                    failed_ = false;
                    engine_->stop();
                }

                co_return;
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;

            std::atomic<size_t> connections_;
    };

    const std::vector<char> test_stress_class::kData =
        std::vector<char>(test_stress_class::kDataToSend, 42);

    int
    test_stress()
    {
        engine engine(event_loop::configs{4});

        test_stress_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }
}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}