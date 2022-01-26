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
 *  @file async_file.hpp
 *
 */

#ifndef ZAB_ASYNC_FILE_HPP_
#define ZAB_ASYNC_FILE_HPP_

#include <coroutine>
#include <cstdint>
#include <fcntl.h>
#include <span>
#include <stdio.h>
#include <string_view>
#include <vector>

#include "zab/engine.hpp"
#include "zab/simple_future.hpp"
#include "zab/strong_types.hpp"
#include "zab/yield.hpp"

namespace zab {

    // todo(DR): to anable all openat features.
    struct directory {
            int dfd_ = AT_FDCWD;
    };

    namespace file {

        /**
         * @brief      File options used to open the file.
         */
        enum class Option {
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
         * @brief      Convert Options to open options.
         *
         * @param[in]  _options  The option to convert.
         *
         * @return     The fopen options.
         */
        static constexpr auto
        open_options(Option _options)
        {
            switch (_options)
            {
                case Option::kRead:
                    return O_RDONLY;

                case Option::kTrunc:
                    return O_WRONLY | O_CREAT | O_TRUNC;

                case Option::kAppend:
                    return O_WRONLY | O_CREAT | O_APPEND;

                case Option::kReadWrite:
                    return O_RDWR;

                case Option::kRWTruncate:
                    return O_RDWR | O_CREAT | O_TRUNC;

                case Option::kRWAppend:
                    return O_RDWR | O_CREAT | O_APPEND;
            }

            return 0;
        }

    }   // namespace file

    /**
     * @brief      This class allows for asyncronous file i/o.
     *
     * @details    The file descriptor associated with async_file is opened as if
     * by call to fopen() or open(2) with the following flags:
     *
     *        ┌───────────────┬───────────────┬───────────────────────────────┐
     *        │  Options      │  fopen() mode │  open() flags                 │
     *        ├───────────────┼───────────────┼───────────────────────────────┤
     *        │ kRead         │     r         │ O_RDONLY                      │
     *        ├───────────────┼───────────────┼───────────────────────────────┤
     *        │ kTrunc        │     w         │ O_WRONLY | O_CREAT | O_TRUNC  │
     *        ├───────────────┼───────────────┼───────────────────────────────┤
     *        │ kAppend       │     a         │ O_WRONLY | O_CREAT | O_APPEND │
     *        ├───────────────┼───────────────┼───────────────────────────────┤
     *        │ kReadWrite    │     r+        │ O_RDWR                        │
     *        ├───────────────┼───────────────┼───────────────────────────────┤
     *        │ kRWTruncate   │     w+        │ O_RDWR | O_CREAT | O_TRUNC    │
     *        ├───────────────├───────────────┼───────────────────────────────┤
     *        │ kRWAppend     │     a+        │ O_RDWR | O_CREAT | O_APPEND   │
     *        └───────────────┴───────────────┴───────────────────────────────┘
     *
     */
    template <typename ReadType = std::byte>
    class async_file {

        public:

            static auto
            relative_dir()
            {
                return directory{.dfd_ = AT_FDCWD};
            };

            /**
             * @brief      Create a async file.
             *
             * @param      _engine   The engine to use.
             */
            async_file(engine* _engine) : engine_(_engine) { }

            async_file(const async_file&) = delete;

            async_file(async_file&& _move) : engine_(_move.engine_), file_(_move.file_)
            {
                _move.file_ = 0;
            }

            /**
             * @brief      Closes the file.
             *
             * @details    The acutal clsoing of the file will be flushed to a background
             * process. `co_await`ing on `close()` is reconmended before deconsturction.
             */
            ~async_file()
            {
                if (file_)
                {
                    auto tmp = file_;
                    file_    = 0;
                    close_in_background(engine_, tmp);
                }
            }

            /**
             * @brief The defaukt mode for opening files is user read/write
             *
             */
            static constexpr mode_t kDefaultMode = S_IRUSR | S_IWUSR;

            simple_future<bool>
            open(std::string_view _path, file::Option _options, mode_t _mode = kDefaultMode)
            {
                return open(relative_dir(), _path, file::open_options(_options), _mode);
            }

            simple_future<bool>
            open(std::string_view _path, int _flags, mode_t _mode)
            {
                return open(relative_dir(), _path, _flags, _mode);
            }

            simple_future<bool>
            open(
                const directory& _dir,
                std::string_view _path,
                file::Option     _options,
                mode_t           _mode = kDefaultMode)
            {
                return open(_dir, _path, file::open_options(_options), _mode);
            }

            simple_future<bool>
            open(
                const directory& _dir,
                std::string_view _path,
                int              _flags,
                mode_t           _mode = kDefaultMode)
            {
                if (_mode == 0 && ((_flags & O_CREAT) || (_flags & O_TMPFILE))) { co_return false; }

                auto rc =
                    co_await engine_->get_event_loop().open_at(_dir.dfd_, _path, _flags, _mode);

                if (rc && *rc >= 0)
                {
                    file_ = *rc;
                    co_return true;
                }
                else
                {
                    co_return false;
                }
            }

            simple_future<bool>
            close()
            {
                auto tmp = file_;
                file_    = 0;
                return close(engine_, tmp);
            }

            static simple_future<bool>
            close(engine* _engine, int _fd)
            {
                struct cu {
                        ~cu()
                        {
                            if (fd_)
                            {
                                if (::close(fd_) < 0)
                                {
                                    std::cerr << "async_file failed to close a file descriptor\n";
                                }
                            }
                        }
                        int& fd_;
                } cleaner{_fd};

                if (_engine->current_id() == thread_t::any_thread())
                {
                    co_await yield(_engine, thread_t::any_thread());
                }

                auto rc = co_await _engine->get_event_loop().close(_fd);
                _fd     = 0;

                if (rc && !*rc) { co_return true; }
                else
                {
                    co_return false;
                }
            }

            static async_function<>
            close_in_background(engine* _engine, int _fd)
            {
                co_await close(_engine, _fd);
            }

            simple_future<std::vector<ReadType>>
            read_file() noexcept
            {
                auto file_size = lseek(file_, 0, static_cast<int>(file::Offset::kEnd));
                lseek(file_, 0, static_cast<int>(file::Offset::kBegin));
                return read_some(file_size);
            }

            simple_future<std::vector<ReadType>>
            read_some(std::size_t _amount) noexcept
            {
                std::vector<ReadType> data(_amount);
                auto                  size = co_await read_some(data);

                if (size)
                {
                    data.resize(*size);
                    co_return data;
                }
                else
                {
                    co_return std::nullopt;
                }
            }

            simple_future<std::size_t>
            read_some(std::span<ReadType> _data) noexcept
            {
                auto        current    = lseek(file_, 0, static_cast<int>(file::Offset::kCurrent));
                const auto  total_size = size_conversion(_data.size());
                std::size_t total      = 0;
                while (total != total_size)
                {
                    auto to_read = std::min<std::size_t>(
                        total_size - total,
                        std::numeric_limits<std::int32_t>::max() - 1);

                    auto rc = co_await engine_->get_event_loop().read(
                        file_,
                        convert(_data, total, to_read),
                        current + total);

                    if (rc && *rc > 0) { total += *rc; }
                    else
                    {
                        break;
                    }
                }

                co_return total;
            }

            simple_future<bool>
            write_to_file(std::span<const ReadType> _data) noexcept
            {
                auto        current    = lseek(file_, 0, static_cast<int>(file::Offset::kCurrent));
                const auto  total_size = size_conversion(_data.size());
                std::size_t total      = 0;
                while (total != total_size)
                {
                    auto to_write = std::min<std::size_t>(
                        total_size - total,
                        std::numeric_limits<std::int32_t>::max() - 1);

                    auto rc = co_await engine_->get_event_loop().write(
                        file_,
                        convert(_data, total, to_write),
                        current + total);

                    if (rc && *rc > 0) { total += *rc; }
                    else
                    {
                        break;
                    }
                }

                co_return total;
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
            position(std::size_t _pos, file::Offset _whence = file::Offset::kBegin) noexcept
            {
                return lseek(file_, _pos, static_cast<int>(_whence)) >= 0;
            }

            std::size_t
            size()
            {
                auto current = lseek(file_, 0, static_cast<int>(file::Offset::kCurrent));
                auto size    = lseek(file_, 0, static_cast<int>(file::Offset::kEnd));
                lseek(file_, current, static_cast<int>(file::Offset::kBegin));
                return size;
            }

            /**
             * @brief      Checks if there is an error pending on the file.
             *
             * @return     true if no error, false otherwise.
             */
            bool
            good() const noexcept
            {
                return (bool) file_;
            }

        private:

            static inline constexpr std::size_t
            size_conversion(std::size_t _size)
            {
                return _size / sizeof(ReadType);
            }

            static inline constexpr std::span<std::byte>
            convert(std::span<ReadType> _data, std::size_t _position, std::size_t _total)
            {
                return std::span<std::byte>((std::byte*) (_data.data() + _position), _total);
            }

            static inline constexpr std::span<const std::byte>
            convert(std::span<const ReadType> _data, std::size_t _position, std::size_t _total)
            {
                return std::span<const std::byte>(
                    (const std::byte*) (_data.data() + _position),
                    _total);
            }

            engine* engine_;
            int     file_;
    };

}   // namespace zab

#endif /* ZAB_ASYNC_FILE_HPP_ */