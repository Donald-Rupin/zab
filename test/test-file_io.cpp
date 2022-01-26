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
 *  @file test-file_io.hpp
 *
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>

#include "zab/async_file.hpp"
#include "zab/async_function.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/simple_promise.hpp"

namespace zab::test {

    static constexpr auto kFileSize = 1028 * 1028 * 50;
    const char*           file_name = "zab-test-read.file";

    int
    test_read();

    int
    test_write();

    int
    test_read_write();

    int
    run_test()
    {
        return test_read() || test_write() || test_read_write();
    }

    class test_read_class : public engine_enabled<test_read_class> {

        public:

            static constexpr auto kInitialiseThread = 0;
            static constexpr auto kDefaultThread    = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                std::vector<char> buffer(kFileSize, 42);
                buffer[kFileSize - 1] = 0;

                { /* Create a file*/
                    std::ofstream outfile(file_name, std::ofstream::binary);

                    outfile << buffer.data();
                }

                buffer.erase(buffer.end() - 1);

                {
                    async_file<char> file(engine_);

                    bool worked = co_await file.open(file_name, file::Option::kRead);

                    if (worked)
                    {

                        auto test_buffer = co_await file.read_file();

                        if (test_buffer && *test_buffer == buffer) { failed_ = false; }
                    }
                }

                ::remove(file_name);

                engine_->stop();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;
    };

    int
    test_read()
    {
        engine engine(engine::configs{1});

        test_read_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_write_class : public engine_enabled<test_write_class> {

        public:

            static constexpr auto kInitialiseThread = 0;
            static constexpr auto kDefaultThread    = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                std::vector<char> buffer(kFileSize, 'Z');
                bool              worked = false;

                {
                    async_file<char> file(engine_);

                    worked = co_await file.open(file_name, file::Option::kTrunc);

                    if (worked) { worked = co_await file.write_to_file(buffer); }

                    co_await file.close();
                }

                if (worked)
                {

                    /* TODO(donald): This takes ages!*/
                    std::ifstream infile(file_name, std::ifstream::binary);

                    std::vector<char> second;
                    second.reserve(kFileSize);

                    std::copy(
                        std::istream_iterator<char>(infile),
                        std::istream_iterator<char>(),
                        std::back_inserter(second));

                    if (second == buffer) { failed_ = false; }
                }

                ::remove(file_name);

                engine_->stop();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;
    };

    int
    test_write()
    {
        engine engine(engine::configs{1});

        test_write_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_read_write_class : public engine_enabled<test_read_write_class> {

        public:

            static constexpr auto kInitialiseThread = 0;
            static constexpr auto kDefaultThread    = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {

                std::vector<char> buffer(kFileSize, 42);

                async_file<char> file(engine_);

                bool worked = co_await file.open(file_name, file::Option::kRWTruncate);

                if (worked)
                {
                    worked = co_await file.write_to_file(buffer);

                    if (worked)
                    {

                        file.position(0);

                        auto test_buffer = co_await file.read_file();

                        if (test_buffer && *test_buffer == buffer) { failed_ = false; }
                    }
                }

                ::remove(file_name);

                engine_->stop();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            bool failed_ = true;
    };

    int
    test_read_write()
    {
        engine engine(engine::configs{1});

        test_read_write_class test;

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
