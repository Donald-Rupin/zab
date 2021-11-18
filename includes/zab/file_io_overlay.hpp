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
 *  @file file_io_overlay.hpp
 *
 */

#ifndef ZAB_FILE_IO_OVERLAY_HPP_
#define ZAB_FILE_IO_OVERLAY_HPP_

#include <coroutine>
#include <cstdint>
#include <span>
#include <stdio.h>
#include <string>
#include <vector>

#include "zab/engine.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"

namespace zab {

    class engine;

    /**
     * @brief      This class allows for asyncronous file i/o.
     *
     * @details    The file descriptor associated with async_file is opened as if
     * by call to fopen() or open(2) with the following flags:
     *
     *        ┌───────────────┬─────────────┬───────────────────────────────┐
     *        │  Options      │fopen() mode │      open() flags             │
     *        ├───────────────┼─────────────┼───────────────────────────────┤
     *        │ kRead         │     r       │ O_RDONLY                      │
     *        ├───────────────┼─────────────┼───────────────────────────────┤
     *        │ kTrunc        │     w       │ O_WRONLY | O_CREAT | O_TRUNC  │
     *        ├───────────────┼─────────────┼───────────────────────────────┤
     *        │ kAppend       │     a       │ O_WRONLY | O_CREAT | O_APPEND │
     *        ├───────────────┼─────────────┼───────────────────────────────┤
     *        │ kReadWrite    │     r+      │ O_RDWR                        │
     *        ├───────────────┼─────────────┼───────────────────────────────┤
     *        │ kRWTruncate   │     w+      │ O_RDWR | O_CREAT | O_TRUNC    │
     *        ├───────────────├─────────────┼───────────────────────────────┤
     *        │ kRWAppend     │     a+      │ O_RDWR | O_CREAT | O_APPEND   │
     *        └───────────────┴─────────────┴───────────────────────────────┘
     *
     */
    class async_file {

        public:

            /* How much data to read every async iteration. */
            static constexpr size_t kBufferSize = 1028 * 1028 * 8;

            /**
             * @brief      File options used to open the file.
             */
            enum class Options {
                kRead,
                kTrunc,
                kAppend,
                kReadWrite,
                kRWTruncate,
                kRWAppend,
            };

            /**
             * @brief      Offests for repositioning the file cursor position.
             */
            enum class Offset {
                kBegin   = SEEK_SET,
                kCurrent = SEEK_CUR,
                kEnd     = SEEK_END
            };

            /**
             * @brief      Create a async file.
             *
             * @param      _engine   The engine to use.
             * @param[in]  _path     The path of the file to open.
             * @param[in]  _options  The options to open the file under.
             */
            async_file(engine* _engine, const std::string& _path, Options _options);

            /**
             * @brief      Closes the file.
             */
            ~async_file();

            /**
             * @brief      Convert async_file::Options to fopen options.
             *
             * @param[in]  _options  The option to convert.
             *
             * @return     The fopen options.
             */
            static constexpr const char*
            fopen_options(Options _options)
            {
                switch (_options)
                {
                    case Options::kRead:
                        return "r";

                    case Options::kTrunc:
                        return "w";

                    case Options::kAppend:
                        return "a";

                    case Options::kReadWrite:
                        return "r+";

                    case Options::kRWTruncate:
                        return "w+";

                    case Options::kRWAppend:
                        return "a+";
                }

                return "";
            }

            /**
             * @brief      Read a file in from disk.
             *
             * @details    Calling this function from within the IO thread will
             * allow this function to skip a thread transfer.
             *
             *             Returning into the IO thread will also allow this
             * function to skip another thread transfer
             *
             *
             * @param[in]  _return_into  Which thread to return into.
             *
             * @return     A future value to be `co_await`'ed.
             */
            simple_future<std::vector<char>>
            read_file(thread_t _return_into) noexcept;

            simple_future<std::vector<char>>
            read_file() noexcept
            {
                return read_file(engine_->get_event_loop().current_id());
            }

            /**
             * @brief      Read some of the file in from disk.
             *
             * @details    Calling this function from within the IO thread will
             * allow this function to skip a thread transfer.
             *
             *             Returning into the IO thread will also allow this
             * function to skip another thread transfer
             *
             * @param[in]  _return_into  Which thread to return into.
             *
             * @return     A future value to be `co_await`'ed.
             */
            simple_future<std::vector<char>>
            read_some(thread_t _return_into, size_t _amount) noexcept;

            simple_future<std::vector<char>>
            read_some(size_t _amount) noexcept
            {
                return read_some(engine_->get_event_loop().current_id(), _amount);
            }

            /**
             * @brief      Writes to the file.
             *
             * @details    The lifetime of _data must last longer then the promise.
             * This is guarenteed if the owner is a local variable of the function
             *             co_awaiting.
             *
             *             Calling this function from within the IO thread will
             * allow this function to skip a thread transfer.
             *
             *             Returning into the IO thread will also allow this
             * function to skip another thread transfer
             *
             * @param[in]  _return_into  Which thread to return into.
             * @param[in]  _data         The data to write.
             *
             * @return     A future value to be `co_await`'ed.
             */
            simple_future<bool>
            write_to_file(thread_t _return_into, std::span<char> _data) noexcept;

            simple_future<bool>
            write_to_file(std::span<char> _data) noexcept
            {
                return write_to_file(engine_->get_event_loop().current_id(), _data);
            }

            /**
             * @brief      Writes to the file.
             *
             * @details    Takes ownership of the _data.
             *
             *             Calling this function from within the IO thread will
             * allow this function to skip a thread transfer.
             *
             *             Returning into the IO thread will also allow this
             * function to skip another thread transfer
             *
             * @param[in]  _return_into  Which thread to return into.
             * @param[in]  _data         The data to write.
             *
             * @return     A future value to be `co_await`'ed.
             */
            simple_future<bool>
            write_to_file(thread_t _return_into, std::vector<char>&& _data) noexcept;

            simple_future<bool>
            write_to_file(std::vector<char>&& _data) noexcept
            {
                return write_to_file(engine_->get_event_loop().current_id(), std::move(_data));
            }

            /**
             * @brief      Reset the poistion of the file cursor.
             *
             * @param[in]  _pos     The amount to move.
             * @param[in]  _whence  Where to move from.
             *
             * @return     true if successful, false otherwise.
             */
            bool
            position(size_t _pos, Offset _whence = Offset::kBegin) noexcept;

            /**
             * @brief      Checks if there is an error pending on the file.
             *
             * @return     true if no error, false otherwise.
             */
            bool
            good() noexcept;

            /**
             * @brief      Get the io thread for the engine. This is #threads - 1.
             *
             * @return     The io thread.
             */
            thread_t
            io_thread() noexcept;

        private:

            engine* engine_;
            FILE*   file_;
    };

}   // namespace zab

#endif /* ZAB_FILE_IO_OVERLAY_HPP_ */