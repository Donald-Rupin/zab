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
 *  TODO(donald): Test when number of threads is greatly larger then  expected.
 *
 *  @file test-async_barrier.cpp
 *
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

#include "zab/async_barrier.hpp"
#include "zab/async_function.hpp"
#include "zab/async_primitives.hpp"
#include "zab/async_semaphore.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/reusable_future.hpp"
#include "zab/strong_types.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_single_thread();

    int
    test_multi_thread();

    int
    run_test()
    {
        return test_single_thread() || test_multi_thread();
    }

    class test_single_thread_class : public engine_enabled<test_single_thread_class> {
        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr auto kRounds = 1000;

            test_single_thread_class(std::uint16_t _threads) : threads_(_threads) { }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                std::shared_ptr<async_latch> latch_;

                failed_ = !(co_await do_singles_plain() && co_await do_singles_async());

                get_engine()->stop();
            }

            simple_future<bool>
            do_singles_plain() noexcept
            {
                async_binary_semaphore sem(get_engine(), false);
                std::atomic<size_t>    count{0};

                async_barrier barrier(
                    get_engine(),
                    threads_,
                    [this, &sem, rounds = 0u]() mutable noexcept
                    {
                        ++rounds;

                        // //std::cout << "Done round: " << rounds << " out of " << threads_ *
                        // kRounds << "\n";
                        if (rounds == threads_ * kRounds) { sem.release(); }
                    },
                    thread_t{0});

                /* Create worker threads... */
                for (std::uint16_t t = 0; t < threads_; ++t)
                {
                    worker_thread(barrier, count, t);
                }

                /* wait for the processing to finish*/
                co_await sem;

                co_await yield();

                co_return count.load() == compute_cycles();
            }

            template <typename T>
            async_function<>
            worker_thread(
                async_barrier<T>&    _barrier,
                std::atomic<size_t>& _count,
                std::uint16_t        _id) noexcept
            {
                int64_t reducer        = _id + 1;
                size_t  internal_count = 1;

                while (reducer)
                {
                    co_await _barrier.arrive_and_wait();

                    ++_count;

                    if (internal_count % kRounds == 0) { --reducer; }

                    co_await yield();

                    ++internal_count;
                }
                if (_id != threads_ - 1) { _barrier.arrive_and_drop(); }
                else
                {
                    ++_count;
                }
            }

            size_t
            compute_cycles() const noexcept
            {
                size_t cycles     = 1;
                size_t count      = 0;
                size_t multiplier = threads_;

                while (multiplier)
                {
                    count += multiplier;
                    if (cycles % kRounds == 0) { --multiplier; }

                    ++cycles;
                }

                return count;
            }

            reusable_future<>
            do_sync_phase(async_binary_semaphore& sem)
            {
                size_t rounds = 0;

                while (rounds != threads_ * kRounds)
                {
                    ++rounds;

                    co_await yield();

                    co_await yield();

                    if (rounds != threads_ * kRounds) { co_yield promise_void{}; }
                }

                sem.release();

                co_return;
            }

            simple_future<bool>
            do_singles_async() noexcept
            {
                async_binary_semaphore sem(get_engine(), false);
                std::atomic<size_t>    count{0};

                async_barrier barrier(get_engine(), threads_, do_sync_phase(sem), thread_t{0});

                /* Create worker threads... */
                for (std::uint16_t t = 0; t < threads_; ++t)
                {
                    worker_thread(barrier, count, t);
                }

                /* wait for the processing to finish*/
                co_await sem;

                co_await yield();

                co_return count.load() == compute_cycles();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            size_t threads_;

            bool failed_ = true;
    };

    int
    test_single_thread()
    {
        auto test_lam = [](std::uint16_t _thread_count)
        {
            engine engine(event_loop::configs{1});

            test_single_thread_class test(_thread_count);

            test.register_engine(engine);

            engine.start();

            return test.failed();
        };

        return test_lam(3) || test_lam(5) || test_lam(8) || test_lam(12) || test_lam(24);
    }

    class test_multi_thread_class : public engine_enabled<test_multi_thread_class> {
        public:

        public:

            static constexpr auto kDefaultThread = 0;
            static constexpr auto kRounds        = 10000;

            test_multi_thread_class(std::uint16_t _threads) : threads_(_threads) { }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                std::shared_ptr<async_latch> latch_;

                failed_ = !(co_await do_singles_plain() && co_await do_singles_async());

                get_engine()->stop();
            }

            simple_future<bool>
            do_singles_plain() noexcept
            {
                async_binary_semaphore sem(get_engine(), false);
                std::atomic<size_t>    count{0};

                async_barrier barrier(
                    get_engine(),
                    threads_,
                    [this, &sem, rounds = 0u]() mutable noexcept
                    {
                        ++rounds;

                        // std::cout << "Done round: " << rounds << " out of " << threads_ * kRounds
                        // << "\n";
                        if (rounds == threads_ * kRounds) { sem.release(); }
                    },
                    thread_t{0});

                /* Create worker threads... */
                for (std::uint16_t t = 0; t < threads_; ++t)
                {
                    worker_thread(barrier, count, t);
                }

                /* wait for the processing to finish*/
                co_await sem;

                for (std::uint16_t t = 0; t < threads_; ++t)
                {
                    co_await yield(now(), thread_t{t});
                }

                // std::cout << count.load() << " == " << compute_cycles() << "\n";
                co_return count.load() == compute_cycles();
            }

            template <typename T>
            async_function<>
            worker_thread(
                async_barrier<T>&    _barrier,
                std::atomic<size_t>& _count,
                std::uint16_t        _id) noexcept
            {
                co_await yield(now(), thread_t{_id});

                if (expected(get_engine()->get_event_loop().current_id(), thread_t{_id}))
                {
                    get_engine()->stop();
                    co_return;
                }

                int64_t reducer        = _id + 1;
                size_t  internal_count = 1;

                while (reducer)
                {
                    // std::cout <<  "DOING WORK " << reducer << "\n";
                    if (expected(get_engine()->get_event_loop().current_id(), thread_t{_id}))
                    {
                        get_engine()->stop();
                        co_return;
                    }

                    co_await _barrier.arrive_and_wait();

                    // std::cout <<  "WAKIGN WORK"<< "\n";

                    if (expected(get_engine()->get_event_loop().current_id(), thread_t{_id}))
                    {
                        get_engine()->stop();
                        co_return;
                    }

                    ++_count;

                    if (internal_count % kRounds == 0) { --reducer; }

                    co_await yield(now(), thread_t{_id});

                    ++internal_count;
                }

                // std::cout <<  "WORK IS OVER!"<< "\n";
                if (_id != threads_ - 1) { _barrier.arrive_and_drop(); }
                else
                {
                    ++_count;
                }
            }

            size_t
            compute_cycles() const noexcept
            {
                size_t cycles     = 1;
                size_t count      = 0;
                size_t multiplier = threads_;

                while (multiplier)
                {
                    count += multiplier;
                    if (cycles % kRounds == 0) { --multiplier; }

                    ++cycles;
                }

                return count + 1;
            }

            reusable_future<>
            do_sync_phase(async_binary_semaphore& sem)
            {
                // std::cout <<  "SYN PHASE!"<< "\n";
                size_t rounds = 0;

                while (rounds != threads_ * kRounds)
                {
                    ++rounds;

                    co_await yield();

                    co_await yield();

                    if (rounds != threads_ * kRounds) { co_yield promise_void{}; }

                    // std::cout <<  "SYN PHASE AWAKE" << rounds << "\n";

                    if (expected(get_engine()->get_event_loop().current_id(), thread_t{0}))
                    {
                        get_engine()->stop();
                        co_return;
                    }
                }

                // std::cout <<  "SYN OVER"<< "\n";
                sem.release();

                co_return;
            }

            simple_future<bool>
            do_singles_async() noexcept
            {
                async_binary_semaphore sem(get_engine(), false);
                std::atomic<size_t>    count{0};

                async_barrier barrier(get_engine(), threads_, do_sync_phase(sem), thread_t{0});

                /* Create worker threads... */
                for (std::uint16_t t = 0; t < threads_; ++t)
                {
                    worker_thread(barrier, count, t);
                }

                /* wait for the processing to finish*/
                co_await sem;

                for (std::uint16_t t = 0; t < threads_; ++t)
                {
                    co_await yield(now(), thread_t{t});
                }

                // std::cout << count.load() << " =1= " << compute_cycles() << "\n";
                co_return count.load() == compute_cycles();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            size_t threads_;

            bool failed_ = true;
    };

    int
    test_multi_thread()
    {
        auto test_lam = [](std::uint16_t _thread_count)
        {
            engine engine(event_loop::configs{(std::uint16_t)(_thread_count + (std::uint16_t) 1u)});

            test_multi_thread_class test(_thread_count);

            test.register_engine(engine);

            engine.start();

            return test.failed();
        };

        return test_lam(3) || test_lam(5) || test_lam(8) || test_lam(12);
    }

}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}