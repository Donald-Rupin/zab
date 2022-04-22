# ZAB - A high performance coroutine executor and asynchronous io framework

![example workflow](https://github.com/Donald-Rupin/zab/actions/workflows/cmake.yml/badge.svg)

[Home Page](https://donald-rupin.github.io/zab/html/index.html)

A high-performance framework for building asynchronous and multi-threaded programs. 

The original goal of this library was to learn the [new coroutines TS](https://en.cppreference.com/w/cpp/coroutine) for C++. I found the most difficult part of the coroutine TS is when you want to develop an asynchronous architecture or "executer/runtime" that can handle re-entrant code across possibly different threads. Thus, **ZAB** was born. 

ZAB uses [io_uring](https://unixism.net/loti/) and [liburing](https://github.com/axboe/liburing) as its backend to provide asynchronous system calls. ZAB so far does not try to provide QoL or improvements on the posix system call API's. However, C++ classes are provided to provide resource management and group similar function calls.   

Contact: donald.rupin@pm.me

- [Documentation](https://donald-rupin.github.io/zab/html/lib/lib_root.html)
    + [Getting Started](https://donald-rupin.github.io/zab/html/lib/getting_started.html)
    + [Building](https://donald-rupin.github.io/zab/html/lib/getting_started.html#building)
    + [Core Library](https://donald-rupin.github.io/zab/html/lib/core_library.html)
    + [Coroutines](https://donald-rupin.github.io/zab/html/lib/coroutines.html)
    + [Asynchronous Primitives](https://donald-rupin.github.io/zab/html/lib/async_primitives.html)
    + [Asynchronous Synchronisation](https://donald-rupin.github.io/zab/html/lib/async_sync.html)
    + [Observables](https://donald-rupin.github.io/zab/html/lib/observable.html)
    + [File IO](https://donald-rupin.github.io/zab/html/lib/file_io.html)
    + [Networking](https://donald-rupin.github.io/zab/html/lib/networking.html)


## Example Usage
```c++
/* An asynchronous function that returns nothing - it can return execution without finishing itself */
zab::async_function<>
your_class::foo();

/* An asynchronous function that can be awaited (can return void too) */
zab::simple_future<bool>
your_class::bar();

/* An asynchronous generator that can return multiple things (can return void too) */
zab::reusable_future<bool>
your_class::baz();

/* Doing asynchronous behavior ( all is non blocking )*/
zab::async_function<>
your_class::example()
{
    /* Async function usage */

    /* trigger a async function */
    /* An async_function function will return execution on its first suspend. */
    foo();

    /* trigger an async function and get the result */
    /* A co_await'ed simple_future will return execution once it has co_return'ed a value. */
    auto value = co_await bar();

    /* Keep getting values */
    /* A co_await'ed reusable_future will return execution once it has co_return'ed or co_yield'ed a value. */
    while (auto f = baz(); !f.complete()) {
        auto value_2 = co_await f;
    }

    /* or inbuilt for_each(...) */
    co_await zab::for_each(
            baz(),
            [](auto _value_2){ /* ... */ }
        );

    /* Async behavior */

    /* yield control for a time (2 seconds) and return in default thread */
    co_await yield(zab::order::in_seconds(2));

    /* yield control and resume in a different thread (thread 2)*/
    co_await yield(zab::thread::in(2));

    /* or both (but resuming in any thread ) */
    co_await yield(zab::order::in_seconds(2), zab::thread::any());

    /* pause this function for an arbitrary amount of time */
    co_await pause(
        [this](auto* _pause_pack) {
            /* Can be resumed at any time... in any thread... */
            _pause_pack->thread_ = zab::thread::in(1);
            unpause(_pause_pack, now());
        });

    /* concurrently await a series of futures */
    auto[result_1, result_2] = co_await wait_for(
            bar(),
            baz()
        );

    /* Custom Lambda based suspension points */
    int result = co_await  suspension_point(
            [this, x = 42]<typename T>(T _handle) noexcept
                {
                    if constexpr (is_ready<T>())
                    {
                        /* Always suspend! */
                        return false;
                    }
                    else if constexpr (is_suspend<T>())
                    {
                        /* Resume strait away! */
                       engine_->resume(_handle);
                    }
                    else if constexpr (is_resume<T>())
                    {
                        /* Return x! */
                        return x;
                    }
                }
        );
    assert(result == 42);

     /* Custom Lambda based stateful suspension points      */
     /* Allows the suspension point to resume internally    */
     /* without resuming what is awaiting on it. Useful for */
     /* handling automatic retries or partial completions.   */
    int result_2 = co_await  stateful_suspension_point<int>(
            [this, x = 42]<typename T>(T _handle) mutable noexcept
                {
                    if constexpr (is_ready<T>())
                    {
                        /* Suspend until counter hits 44 */
                        return x == 44;
                    }
                    else if constexpr (is_stateful_suspend<int, T>())
                    {
                        /* Resume with data set to x + 1 */
                        assert(x >= 42 && x < 44>);
                        _handle->result_ = x + 1;
                        engine_->resume(_handle->event_);

                    } else if constexpr (is_notify<int, T>())
                    {
                        /* Something resumed us with value _handle */
                        x = _handle;
                        /* Return from is_ready<T>() */
                        return notify_ctl::kReady;
                    }
                    else if constexpr (is_resume<T>())
                    {
                        /* Return x! */
                        return x;
                    }
                }
        );
    assert(result_2 == 44);

    /* Observable */

    zab::observable<std::string, int> ob(engine_);

    auto con = ob.connect();

    /* emit a value asynchronously */
    ob.async_emit("hello", 4);

    /* or emit and wait for all observers to receive */
    co_await ob.emit("world", 2);

    {
        /* Emits are 0 copy, all observers will get the same object  */
        auto e = co_await con;

        const auto&[e_string, e_int] = e.event();

        /* Always received in order of emit */
        assert(e_string == "hello" && e_int == 4);

        /* Deconstruction of objects is guarded by e. Once all     */
        /* observer destroy e, the event objects are deconstructed */
        /* An observable waiting on an emit will wake once all e's */
        /* are deconstructed. */
    }

    /* We can do some non-blocking synchronisation */

    /* mutex - for mutual exclusion */
    zab::async_mutex mtx(engine_);

    {
        /* Acquire a scoped lock */
        auto lck = co_await mtx;
    }

    /* binary semaphore - for signalling - created in locked mode  */
    zab::async_binary_semaphore sem(engine_, false);

    /* release the sem */
    sem.release();

    /* acquire the sem */
    co_await sem;

    /* Lots more synchronization primitives in the library... */

    /* File IO */
    zab::async_file<char> file(engine_);

    auto success = co_await file.open("test_file.txt", file::Option::kRWTruncate);

    std::vector<char> buffer(42, 42);
    /* write to file! */
    bool success = co_await file.write_to_file(buffer);
    if (success)
    {
        /* Reset position and read from file  */
        file.position(0);
        std::optional<std::vector<char>> data = co_await file.read_file();
    }

    /* Networking */

    /* acceptors or connectors make tcp streams! */
    zab::tcp_acceptor acceptor(engine_);

    struct sockaddr_storage _address;
    socklen_t               _length = sizeof(_address);
    if (acceptor_.listen(AF_INET, 8080, 10))
    {
        while (!acceptor_.last_error())
        {
            auto stream = co_await acceptor_.accept<char>((struct sockaddr*) &_address, &_length);

            if (stream) {

                std::vector<char> buf(5);

                auto amount_wrote = co_await stream->write(buf);

                auto amount_read  = co_await stream->read(buf);

                co_await stream->shutdown();

                co_await stream->close();
            }
        }
    }

}
```