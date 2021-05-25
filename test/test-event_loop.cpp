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
 *  @file test-event_loop.cpp
 *
 */

#include <iostream>

#include "zab/event.hpp"
#include "zab/event_loop.hpp"
#include "zab/strong_types.hpp"

#include "internal/macros.hpp"

namespace zab::test {

    int
    test_basic();

    int
    test_multi_thread_basic();

    int
    test_run_time_basic();

    /**
     * @brief      run all the tests.
     *
     * @return     0 if successful, not 0 otherwise
     */
    int
    run_test()
    {
        return test_basic() || test_multi_thread_basic() || test_run_time_basic();
    }

    /**
     * @brief      Test that events are executed in ordering specified.
     *
     * @return     0 if successful, not 0 otherwise
     */
    int
    test_basic()
    {

        int        failed = -1;
        int        count  = 0;
        event_loop el(event_loop::configs{});

        /* Send 2nd event */
        el.send_event(
            event{
                .type_ =
                    code_block{
                        .cb_ =
                            [&](auto)
                        {
                            /* This event will happen
                             * second so count will be 1
                             */
                            if (expected(1, count))
                            {
                                failed = 1;
                                el.stop();
                            }

                            ++count;
                        }}},
            order_t{1},
            thread_t{0});

        /* Send 3rd event */
        el.send_event(
            event{
                .type_ =
                    code_block{
                        .cb_ =
                            [&](auto)
                        {
                            /* This event will happen
                             * third so count will be 2
                             */
                            if (expected(2, count)) { failed = 2; }
                            else
                            {
                                failed = 0;
                            }
                            ++count;
                            el.stop();
                        },
                    },
            },
            order_t{2},
            thread_t{0});

        /* Send 1st event */
        el.send_event(
            event{
                .type_ =
                    code_block{
                        .cb_ =
                            [&](auto)
                        {
                            /* This event will happen
                             * first so count will be 0
                             */
                            if (expected(0, count))
                            {
                                failed = 1;
                                el.stop();
                            }

                            ++count;
                        }}},
            order_t{0},
            thread_t{0});

        /* Begin processing events... */
        el.start();

        return failed;
    }

    /**
     * @brief      This will test that events targeting separate threads wont
     * block execution of other threads.
     *
     *             thread 2 will spin lock until thread 1 has completed all
     * events. thread 3 will spin lock until thread 2 has completed all events.
     *
     *             All events will be executed in order of them being added.
     *
     * @return     0 if successful, not 0 otherwise
     */
    int
    test_multi_thread_basic()
    {
        /* Number of events to do in each thread. */
        static constexpr int kLoops = 100;
        int                  failed = -1;
        std::atomic<int>     count  = {0};

        /* Need at lest 3 event loops... */
        event_loop el(event_loop::configs{3});

        /* thread 1 events */
        for (const auto i : std::views::iota(0, kLoops))
        {
            el.send_event(
                event{
                    .type_ =
                        code_block{
                            .cb_ =
                                [&, i](auto)
                            {
                                if (expected(i, count.load()))
                                {
                                    failed = count.load();
                                    el.stop();
                                }

                                ++count;
                            }}},
                order_t{i},
                thread_t{0});
        }

        /* thread 2 events */
        for (const auto i : std::views::iota(0, kLoops))
        {
            el.send_event(
                event{
                    .type_ =
                        code_block{
                            .cb_ =
                                [&, i](auto)
                            {
                                while (count.load() < kLoops)
                                { /* Spin! */
                                };

                                if (expected(i + kLoops, count.load()))
                                {
                                    failed = count.load();
                                    el.stop();
                                }

                                ++count;
                            }}},
                order_t{i},
                thread_t{1});
        }

        /* thread 3 events */
        for (const auto i : std::views::iota(0, kLoops))
        {
            el.send_event(
                event{
                    .type_ =
                        code_block{
                            .cb_ =
                                [&, i](auto)
                            {
                                while (count.load() < (kLoops * 2))
                                { /* Spin! */
                                };

                                if (expected(i + (kLoops * 2), count.load()))
                                {
                                    failed = count.load();
                                    el.stop();
                                }

                                ++count;

                                /* Finish when completed all
                                 * events */
                                if (count.load() == kLoops * 3)
                                {
                                    failed = 0;
                                    el.stop();
                                }
                            }}},
                order_t{i},
                thread_t{2});
        }

        el.start();

        return failed;
    }

    /**
     * @brief      Helper function to loop within the event loop.
     *
     * @param        _el      The event loop.
     * @param[in]    _count   The amount of times its looped.
     * @param[out]   _failed  The failed flag.
     */
    void
    run_time_helper(event_loop& _el, int _count, int& _failed)
    {
        static constexpr int kRecursiveLoops = 1000;
        if (_count < kRecursiveLoops)
        {

            _el.send_event(
                event{
                    .type_ =
                        code_block{
                            .cb_ =
                                [&, _count](auto)
                            {
                                run_time_helper(_el, _count + 1, _failed);
                            }}},
                order_t{1},
                thread_t{(std::uint16_t) (_count % _el.number_of_workers())});
        }
        else
        {

            _el.stop();

            _failed = 0;
        }
    }

    /**
     * @brief      Tests adding of events in the runtime across different
     * threads.
     *
     *
     * @return     0 if successful, not 0 otherwise
     */
    int
    test_run_time_basic()
    {

        int        failed = -1;
        event_loop el(event_loop::configs{});

        el.send_event(
            event{
                .type_ =
                    code_block{
                        .cb_ =
                            [&](auto)
                        {
                            run_time_helper(el, 0, failed);
                        }},
            },
            order_t{1},
            thread_t{0});

        el.start();

        return failed;
    }

}   // namespace zab::test

int
main()
{
    return zab::test::run_test();
}