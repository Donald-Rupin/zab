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
 *  @file test-async_semaphore.cpp
 *
 */

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>

#include "zab/async_function.hpp"
#include "zab/async_latch.hpp"
#include "zab/async_semaphore.hpp"
#include "zab/engine.hpp"
#include "zab/engine_enabled.hpp"
#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/signal_handler.hpp"
#include "zab/strong_types.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_binary_not_pause();

    int
    test_binary_multi_thread_mutex();

    int
    test_counting_single_thread();

    int
    test_counting_multi_thread();

    int
    run_test()
    {
        return test_binary_not_pause() || test_binary_multi_thread_mutex() ||
               test_counting_single_thread() || test_counting_multi_thread();
    }

    class test_binary_not_pause_class : public engine_enabled<test_binary_not_pause_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                sem_ = std::make_shared<async_binary_semaphore>(engine_, true);

                count_ = 0;

                no_defer();

                count_ = 1;

                /* This will lock until `Acquire()` until it has completed */
                co_await *sem_;
                sem_->release();

                co_await in_order();

                co_await *sem_;
                sem_->release();

                if (expected(count_, 4u)) { engine_->stop(); }
                else
                {

                    failed_ = false;
                }

                engine_->stop();
            }

            async_function<>
            no_defer() noexcept
            {
                co_await *sem_;
                sem_->release();

                if (expected(count_, 0u)) { engine_->stop(); }

                co_await *sem_;
                sem_->release();

                if (expected(count_, 0u)) { engine_->stop(); }

                co_await *sem_;
                lock();
                sem_->release();

                if (expected(count_, 0u)) { engine_->stop(); }
            }

            async_function<>
            lock() noexcept
            {
                co_await *sem_;

                if (expected(count_, 1u)) { engine_->stop(); }

                sem_->release();
            }

            simple_future<>
            in_order() noexcept
            {

                co_await *sem_;

                /* (1) */
                add_one(1);

                sem_->release();

                /* (1) Wont have been woken up yet... (cause there is only 1 thread)*/
                if (expected(count_, 1u)) { engine_->stop(); }

                /* But now we wait on (1) */
                co_await *sem_;

                /* (2) */
                add_one(2);

                sem_->release();

                /* (2) Wont have been woken up yet... (cause there is only 1 thread)*/
                if (expected(count_, 2u)) { engine_->stop(); }

                /* But now we wait on (2) */
                co_await *sem_;

                /* (3) */
                add_one(3);

                sem_->release();

                /* (3) Wont have been woken up yet... (cause there is only 1 thread)*/
                if (expected(count_, 3u)) { engine_->stop(); }
            }

            async_function<>
            add_one(size_t _expected) noexcept
            {
                co_await *sem_;
                if (expected(count_, _expected)) { engine_->stop(); }

                ++count_;

                sem_->release();
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            std::shared_ptr<async_binary_semaphore> sem_;

            size_t count_ = 0;

            bool failed_ = true;
    };

    int
    test_binary_not_pause()
    {
        engine engine(engine::configs{2});

        test_binary_not_pause_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class binary_multi_thread_mutex_class : public engine_enabled<binary_multi_thread_mutex_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr std::uint16_t kNumberThreads = 10;
            static constexpr size_t        kNumberOpps    = 500;

            void
            initialise() noexcept
            {
                sem_ = std::make_shared<async_binary_semaphore>(engine_, true);
                run();
            }

            async_function<>
            run() noexcept
            {
                co_await yield();

                for (std::uint16_t i = 0; i < kNumberThreads; ++i)
                {
                    do_thread_lock(thread_t{i});
                }
            }

            async_function<>
            do_thread_lock(thread_t _thread) noexcept
            {
                /* Go into our thread */
                co_await yield(now(), _thread);

                for (size_t i = 0; i < kNumberOpps; ++i)
                {

                    /* Acquire lock */
                    co_await *sem_;

                    /* Acquire resource */
                    current_thread_ = _thread;

                    for (size_t j = 0; j < (kNumberOpps / 2) + 2; ++j)
                    {

                        /* Do some async stuff*/
                        co_await yield(now(), _thread);

                        /* Is still ours! */
                        if (expected(current_thread_, _thread)) { engine_->stop(); }

                        sem_->release();

                        co_await yield(now(), _thread);

                        co_await *sem_;

                        current_thread_ = _thread;
                    }

                    sem_->release();
                }

                if (count_.fetch_add(1) == kNumberThreads - 1)
                {
                    engine_->stop();
                    failed_ = false;
                }
            }

            bool
            failed()
            {
                return failed_;
            }

        private:

            std::shared_ptr<async_binary_semaphore> sem_;

            thread_t current_thread_ = thread_t{0};

            std::atomic<std::uint16_t> count_ = 0;

            bool failed_ = true;
    };

    int
    test_binary_multi_thread_mutex()
    {
        engine engine(engine::configs{binary_multi_thread_mutex_class::kNumberThreads});

        binary_multi_thread_mutex_class test;

        test.register_engine(engine);

        engine.start();

        return test.failed();
    }

    class test_counting_single_thread_class
        : public engine_enabled<test_counting_single_thread_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            test_counting_single_thread_class(size_t _threads) : threads_(_threads) { }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                sem_ = std::make_shared<async_counting_semaphore<>>(engine_, 0);

                failed_ =
                    !(co_await simple_wind() && co_await no_block() && co_await full_release());

                if (!--test_count_) { engine_->stop(); }
            }

            simple_future<bool>
            simple_wind()
            {
                async_latch latch(engine_, threads_ + 1);

                for (size_t i = 0; i < threads_; ++i)
                {
                    acquire_count(latch);
                }

                co_await yield();

                for (size_t i = 0; i < threads_; ++i)
                {

                    sem_->release();

                    /* Since this is single threaded, */
                    /* Our yield should wait until after the coroutine  */
                    /* that was released finishes...  */
                    co_await yield();

                    if (expected(count_, i + 1)) { co_return false; }
                }

                /* Ensure we are synchronized... */
                co_await latch.arrive_and_wait();

                count_ = 0;

                co_return true;
            }

            simple_future<bool>
            no_block()
            {
                /* This should not block at all */
                /* Since we made room for all threads */
                sem_->release(threads_);

                co_await yield();

                async_latch latch(engine_, threads_ + 1);
                for (size_t i = 0; i < threads_; ++i)
                {
                    acquire_count(latch);
                }

                if (expected(count_, threads_)) { co_return false; }

                co_await yield();

                /* Ensure we are synchronized... */
                co_await latch.arrive_and_wait();

                /* try_aquire should fail... */
                auto lock_attempt = sem_->try_aquire();

                if (expected(lock_attempt, false))
                {
                    sem_->release();
                    co_return false;
                }

                count_ = 0;

                co_return true;
            }

            simple_future<bool>
            full_release()
            {
                async_latch latch(engine_, threads_ + 1);

                for (size_t i = 0; i < threads_; ++i)
                {
                    acquire_count(latch);
                }

                co_await yield();

                /* release them all at the same time! */
                sem_->release(threads_);

                /* Ensure we are synchronized... */
                co_await latch.arrive_and_wait();

                if (expected(count_, threads_)) { co_return false; }

                count_ = 0;

                co_return true;
            }

            async_function<>
            acquire_count(async_latch& _latch) noexcept
            {
                co_await *sem_;

                ++count_;

                _latch.count_down();
            }

            bool
            failed()
            {
                return failed_;
            }

            static size_t test_count_;

        private:

            size_t threads_;

            std::shared_ptr<async_counting_semaphore<>> sem_;

            thread_t current_thread_ = thread_t{0};

            std::uint16_t count_ = 0;

            bool failed_ = true;
    };

    size_t test_counting_single_thread_class::test_count_ = 0;

    int
    test_counting_single_thread()
    {
        engine engine(engine::configs{1});

        test_counting_single_thread_class test2(2);
        test_counting_single_thread_class test5(5);
        test_counting_single_thread_class test9(9);
        test_counting_single_thread_class test16(16);
        test_counting_single_thread_class test21(21);

        test2.register_engine(engine);
        test5.register_engine(engine);
        test9.register_engine(engine);
        test16.register_engine(engine);
        test21.register_engine(engine);

        test_counting_single_thread_class::test_count_ = 5;

        engine.start();

        return test2.failed() || test5.failed() || test9.failed() || test16.failed() ||
               test21.failed();
    }

    class test_counting_multi_thread_class
        : public engine_enabled<test_counting_multi_thread_class> {

        public:

            static constexpr auto kDefaultThread = 0;

            static constexpr auto kThreadOpps = 10000;

            test_counting_multi_thread_class(std::uint16_t _threads)
                : threads_(_threads), total_(0), counter_(0)
            { }

            void
            initialise() noexcept
            {
                run();
            }

            async_function<>
            run() noexcept
            {
                /* Allow a third of threads to access */
                sem_ = std::make_shared<async_counting_semaphore<>>(engine_, threads_ / 3);

                for (std::uint16_t fake = 0; fake < threads_; ++fake)
                {
                    for (std::uint16_t real = 0; real < threads_; ++real)
                    {
                        run_thread(thread_t{real});
                    }
                }

                co_return;
            }

            async_function<>
            run_thread(thread_t _thread)
            {
                co_await yield(now(), _thread);

                co_await *sem_;

                auto count = counter_++;

                if (count > (threads_ / 3) - 1)
                {
                    failed_ = false;
                    engine_->stop();
                    co_return;
                }

                if (expected(engine_->current_id(), _thread))
                {
                    sem_->release();
                    co_return;
                }

                for (size_t op = 0; op < kThreadOpps; ++op)
                {

                    --counter_;

                    sem_->release();

                    co_await yield(now(), _thread);

                    co_await *sem_;

                    ++counter_;

                    if (count > (threads_ / 3) - 1)
                    {
                        failed_ = false;
                        engine_->stop();
                        co_return;
                    }

                    if (expected(engine_->current_id(), _thread))
                    {
                        sem_->release();
                        co_return;
                    }

                    co_await yield(now(), _thread);
                }

                --counter_;

                sem_->release();

                auto amount = total_++;

                if (amount == (threads_ * 2) - 1)
                {
                    failed_ = false;
                    engine_->stop();
                }
            }

            bool
            failed()
            {
                return failed_;
            }

            static int test_count_;

        private:

            std::uint16_t threads_;

            std::atomic<int> total_;
            std::atomic<int> counter_;

            std::shared_ptr<async_counting_semaphore<>> sem_;

            thread_t current_thread_ = thread_t{0};

            std::uint16_t count_ = 0;

            bool failed_ = true;
    };

    int
    test_counting_multi_thread()
    {
        auto lam = [](std::uint16_t _thread)
        {
            engine engine(engine::configs{
                .threads_         = (std::uint16_t)(_thread + 1),
                .opt_             = engine::configs::kAtLeast,
                .affinity_set_    = false,
                .affinity_offset_ = 0});

            test_counting_multi_thread_class test1(_thread);
            test1.register_engine(engine);
            engine.start();

            return test1.failed();
        };

        return lam(6) || lam(13) || lam(18) || lam(24);
    }

}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}