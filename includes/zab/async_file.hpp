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
#include "zab/generic_awaitable.hpp"
#include "zab/memory_type.hpp"
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
     * @brief      This class allows for asynchronous file i/o.
     *
     * @details    async_file does not provide any synchronisation for file ops.
     *             async_file must be used within an engine thread.
     *
     *        The file descriptor associated with async_file is opened as if
     *        by call to fopen() or open(2) with the following flags:
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
    template <MemoryType ReadType = std::byte>
    class async_file {

        public:

            /**
             * @brief Dummy directory to specify the path is relative.
             *
             * @return directory
             */
            static auto
            relative_dir()
            {
                return directory{.dfd_ = AT_FDCWD};
            }

            /**
             * @brief      Create a async file.
             *
             * @param      _engine   The engine to use.
             */
            async_file(engine* _engine) : engine_(_engine) { }

            /**
             * @brief Files are not copyable.
             *
             */
            async_file(const async_file&) = delete;

            /**
             * @brief Construct a new async_file object taking the file descriptor from another
             *        async_file.
             *
             * @param _move The async_file to move.
             */
            async_file(async_file&& _move) : engine_(_move.engine_), file_(_move.file_)
            {
                _move.file_ = 0;
            }

            /**
             * @brief Move assignment for an async_file.
             *
             * @param _move_op
             * @return async_file&
             */
            async_file&
            operator=(async_file&& _move_op)
            {
                engine_ = _move_op.engine_;
                if (file_) { close_in_background(engine_, file_); }

                file_          = _move_op.file_;
                _move_op.file_ = 0;
            }

            /**
             * @brief      Closes the file.
             *
             * @details    The actual closing of the file will be flushed to a background
             * process. `co_await`ing on `close()` is reconmended before deconstruction.
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
             * @brief The default mode for opening files is user read/write
             *
             */
            static constexpr mode_t kDefaultMode = S_IRUSR | S_IWUSR;

            /**
             * @brief Opens a file relative to this proccess cwd.
             *
             * @param _path The relative path of the file.
             * @param _options The zab options to apply.
             * @param _mode The open mode.
             * @return suspension_point The awaitable instance for opening the file.
             *                      Async returns the success.
             */
            auto
            open(
                std::string_view _path,
                file::Option     _options,
                mode_t           _mode = kDefaultMode) noexcept
            {
                return open(relative_dir(), _path, file::open_options(_options), _mode);
            }

            /**
             * @brief Opens a file relative to this proccess cwd.
             *
             * @param _path The relative path of the file.
             * @param _flags The flags to apply.
             * @param _mode The open mode.
             * @return suspension_point The awaitable instance for opening the file.
             *                      Async returns the success.
             */
            auto
            open(std::string_view _path, int _flags, mode_t _mode) noexcept
            {
                return open(relative_dir(), _path, _flags, _mode);
            }

            /**
             * @brief Opens a file relative to the directory given.
             *
             * @param directory The directory.
             * @param _path The relative path of the file.
             * @param _options The zab options to apply.
             * @param _mode The open mode.
             * @return suspension_point The awaitable instance for opening the file.
             *                      Async returns the success.
             */
            auto
            open(
                const directory& _dir,
                std::string_view _path,
                file::Option     _options,
                mode_t           _mode = kDefaultMode) noexcept
            {
                return open(_dir, _path, file::open_options(_options), _mode);
            }

            /**
             * @brief Opens a file relative to the directory given.
             *
             * @param directory The directory.
             * @param _path The relative path of the file.
             * @param _flags The flags to apply.
             * @param _mode The open mode.
             * @return suspension_point The awaitable instance for opening the file.
             *                      Async returns the success.
             */
            auto
            open(
                const directory& _dir,
                std::string_view _path,
                int              _flags,
                mode_t           _mode = kDefaultMode) noexcept
            {
                return suspension_point(
                    [this, ret = io_handle{}, dfd = _dir.dfd_, _path, _flags, _mode]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            engine_->get_event_loop().open_at(
                                create_io_ptr(&ret, kHandleFlag),
                                dfd,
                                _path,
                                _flags,
                                _mode);
                        }
                        else if constexpr (is_ready<T>())
                        {
                            return _mode == 0 && ((_flags & O_CREAT) || (_flags & O_TMPFILE));
                        }
                        else
                        {
                            if (ret.result_ > 0)
                            {
                                file_ = ret.result_;
                                return true;
                            }
                            else
                            {
                                return false;
                            }
                        }
                    });
            }

            /**
             * @brief Opens a file relative to the directory given.
             *
             * @param _engine The engine to use.
             * @param directory The directory.
             * @param _path The relative path of the file.
             * @param _flags The flags to apply.
             * @param _mode The open mode.
             * @return suspension_point The awaitable instance for opening the file.
             *                      Async returns the success.
             */
            static auto
            open(
                engine*          _engine,
                const directory& _dir,
                std::string_view _path,
                int              _flags,
                mode_t           _mode = kDefaultMode)
            {
                return suspension_point(
                    [ret = io_handle{}, _engine, dfd = _dir.dfd_, _path, _flags, _mode]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            _engine->get_event_loop().open_at(
                                create_io_ptr(&ret, kHandleFlag),
                                dfd,
                                _path,
                                _flags,
                                _mode);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            std::optional<async_file> result;
                            if (ret.result_ > 0)
                            {
                                result.emplace(_engine);
                                result->file_ = ret.result_;
                            }

                            return result;
                        }
                    });
            }

            /**
             * @brief Attempts to close the file.
             *
             * @return suspension_point The awaitable instance for closing the file.
             *                      Async returns the success.
             */
            auto
            close() noexcept
            {
                auto tmp = file_;
                file_    = 0;
                return close(engine_, tmp);
            }

            /**
             * @brief Attempts to close the file.
             *
             * @param _engine The engine to operate within.
             * @param _fd The file descriptor to close.
             * @return suspension_point The awaitable instance for closing the file.
             *                      Async returns the success.
             */
            static auto
            close(engine* _engine, int _fd) noexcept
            {
                return suspension_point(
                    [ret = io_handle{.handle_ = nullptr, .result_ = -1}, _engine, _fd]<typename T>(
                        T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            _engine->get_event_loop().close(create_io_ptr(&ret, kHandleFlag), _fd);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            return !ret.result_;
                        }
                    });
            }

            /**
             * @brief Attempts to close the file in the background.
             *
             * @details Cannot report failures to the user.
             *
             * @param _engine The engine to operate within.
             * @param _fd The file descriptor to close.
             * @return async_function<> The async function that is running.
             */
            static async_function<>
            close_in_background(engine* _engine, int _fd)
            {
                auto s = co_await close(_engine, _fd);
                if (!s)
                {
                    if (::close(_fd) < 0)
                    {
                        std::cerr << "async_file failed to close a file descriptor\n";
                    }
                }
            }

            /**
             * @brief Reads the entire files contents into a vector.
             *
             * @return simple_future<std::vector<ReadType>>
             */
            simple_future<std::vector<ReadType>>
            read_file() noexcept
            {
                auto file_size = lseek(file_, 0, static_cast<int>(file::Offset::kEnd));
                lseek(file_, 0, static_cast<int>(file::Offset::kBegin));

                std::vector<ReadType> data(file_size);

                decltype(file_size) current = 0;
                while (current != file_size)
                {
                    auto size = co_await read_some(data, current);

                    if (size) { current += *size; }
                    else
                    {
                        data.resize(current);
                        break;
                    }
                }

                co_return data;
            }

            /**
             * @brief Reads up to _amount bytes of data from the file.
             *
             * @details May read less then _amount bytes.
             *
             * @param _amount The maximum amount to read.
             * @return suspension_point The awaitable instance for reading some data.
             *                      Async returns the amount of bytes read.
             */
            auto
            read_some(std::int32_t _amount) noexcept
            {
                return suspension_point(
                    [this,
                     ret  = io_handle{.handle_ = nullptr, .result_ = -1},
                     data = std::vector<ReadType>(_amount)]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            ret.handle_ = _handle;
                            engine_->get_event_loop().read(
                                create_io_ptr(&ret, kHandleFlag),
                                file_,
                                convert(data, data.size()),
                                0);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            std::optional<std::vector<ReadType>> result;
                            if (ret.result_ >= 0)
                            {
                                data.resize(ret.result_);
                                result.emplace(std::move(data));
                            }

                            return result;
                        }
                    });
            }

            /**
             * @brief Reads up to `_data - _off_set` bytes of data from the file.
             *
             * @details May read less then `_data - _off_set` bytes.
             *
             * @param _data The buffer to read data into.
             * @param _off_set The offset for where to read data into the buffer.
             * @return suspension_point The awaitable instance for reading some data.
             *                      Async returns the amount of bytes read.
             */
            auto
            read_some(std::span<ReadType> _data, std::int32_t _off_set = 0) noexcept
            {
                return suspension_point(
                    [this,
                     ret = io_handle{.handle_ = nullptr, .result_ = -1},
                     _data,
                     _off_set]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            auto to_read = std::min<std::size_t>(
                                _data.size() - _off_set,
                                std::numeric_limits<std::int32_t>::max() - 1);

                            ret.handle_ = _handle;
                            engine_->get_event_loop().read(
                                create_io_ptr(&ret, kHandleFlag),
                                file_,
                                convert(_data, _off_set + to_read),
                                _off_set);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            std::optional<std::size_t> result;
                            if (ret.result_ >= 0) { result.emplace(std::move(ret.result_)); }

                            return result;
                        }
                    });
            }

            /**
             * @brief Write the content of _data to the file.
             *
             * @details Only writes less then _data.size bytes if the write operation fails.
             *
             * @param _data The buffer to write from.
             * @return guaranteed_future<std::size_t>
             */
            guaranteed_future<std::size_t>
            write_to_file(std::span<const ReadType> _data) noexcept
            {
                const std::size_t total   = _data.size();
                std::size_t       current = 0;
                while (current != total)
                {
                    auto rc = co_await write_some(_data, current);

                    if (rc && *rc > 0) { current += *rc; }
                    else
                    {
                        break;
                    }
                }

                co_return current;
            }

            /**
             * @brief Writes up to `_data.size() - _off_set` bytes of data to the file.
             *
             * @details May write less then `_data - _off_set` bytes without an error occuring.
             *
             * @param _data The buffer to write data from.
             * @param _off_set The offset for where to write data from the buffer.
             * @return suspension_point The awaitable instance for writing some data.
             *                      Async returns the amount of bytes written.
             */
            auto
            write_some(std::span<const ReadType> _data, std::int32_t _off_set = 0) noexcept
            {
                return suspension_point(
                    [this,
                     ret = io_handle{.handle_ = nullptr, .result_ = -1},
                     _data,
                     _off_set]<typename T>(T _handle) mutable noexcept
                    {
                        if constexpr (is_suspend<T>())
                        {
                            auto to_write = std::min<std::size_t>(
                                _data.size() - _off_set,
                                std::numeric_limits<std::int32_t>::max() - 1);

                            ret.handle_ = _handle;
                            engine_->get_event_loop().write(
                                create_io_ptr(&ret, kHandleFlag),
                                file_,
                                convert(_data, _off_set + to_write),
                                _off_set);
                        }
                        else if constexpr (is_resume<T>())
                        {
                            std::optional<std::size_t> result;
                            if (ret.result_ >= 0) { result.emplace(std::move(ret.result_)); }

                            return result;
                        }
                    });
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

            /**
             * @brief Computes the size of the file.
             *
             * @return std::size_t The size of the file.
             */
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

            static inline constexpr std::span<std::byte>
            convert(std::span<ReadType> _data, std::size_t _total)
            {
                return std::span<std::byte>((std::byte*) (_data.data()), _total);
            }

            static inline constexpr std::span<const std::byte>
            convert(std::span<const ReadType> _data, std::size_t _total)
            {
                return std::span<const std::byte>((const std::byte*) (_data.data()), _total);
            }

            engine* engine_;
            int     file_;
    };

}   // namespace zab

#endif /* ZAB_ASYNC_FILE_HPP_ */