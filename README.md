# ZAB - A coroutine driven asynchronous framework.

![example workflow](https://github.com/Donald-Rupin/zab/actions/workflows/cmake.yml/badge.svg)

An asynchronous framework for building event-driven, multi-threaded programs. 

The original goal of this library was to learn the [new coroutines TS](https://en.cppreference.com/w/cpp/coroutine) for C++. I found the most difficult part of the coroutine TS is when you want to develop an asynchronous architecture or "executer/runtime" that can handle re-entrant code across possibly different threads. Thus, **ZAB** was born. 

The goal then became to provide a generic framework so users can utilise coroutines to easily develop concise, multi-threaded asynchronous code. 

The framework is fairly opinionated with the aim of providing multi-threaded and asynchronous functionality in a way that works like within high-level languages with built-in async support. The design largely takes inspiration from [libuv](https://github.com/libuv/libuv) and its use with javascript in the [node.js](https://nodejs.org/en/) runtime.   

Contact: donald.rupin@pm.me

## Disclosure 

**ZAB** is still very much in the _alpha_ stage so all feedback and suggestions are welcome and encouraged. I do not expect the interface to change significantly but it is possible. Tests are provided and I've tried to be thorough but I don't have the capacity to test everything at this stage. If people are interested in using the **ZAB** framework, I will work to release a fully stable version. 

## Library Contents
- [Building](#Building)
- [Examples](#Examples)
- [Core Library](#Core-Library)
    + [`engine`](#engine)
    + [Ordering and Threads](#Ordering-and-Threads)
    + [`engine_enabled<Base>`](#engine_enabled<Base>)
    + [Descriptor Notifications](#Descriptor-Notifications)
    + [Signal Handling](#Signal-Handling)
- [Coroutine Types](#Coroutine-Types)
    + [`async_function`](#async_function)
    + [`simple_future<T>`](#simple_future<T>)
    + [`reusable_future<T>`](#reusable_future<T>)
- [Promise Types](#Promise-Types)
    + [`execution_promise`](#execution_promise)
    + [`defer_block_promise`](#defer_block_promise)
    + [`simple_promise<T>`](#simple_promise<T>)
    + [`visitor_promise<T>`](#visitor_promise<T>)
    + [`reusable_promise<T>`](#reusable_promise<T>)
- [Asynchronous Primitives](#Asynchronous-Primitives)
    + [`yield`](#yield)
    + [`pause`](#pause)
    + [`wait_for`](#wait_for)
    + [`proxy`](#proxy)
- [Asynchronous Synchronisation](#Asynchronous-Synchronisation)
    + [`pause_token`](#pause_token)
    + [`async_counting_semaphore`](#async_counting_semaphore)
    + [`async_binary_semaphore`](#async_binary_semaphore)
    + [`async_mutex`](#async_mutex)
    + [`async_latch`](#async_latch)
    + [`async_barrier`](#async_barrier)
- [Asynchronous Overlays](#Asynchronous-Overlays)
    + [`File IO`](#File-IO)
    + [`Networking`](#Networking)

## Building

Portability has not been the primary focus in the creation of **ZAB**. Any and all help with porting and verifying ZAB works across different platforms and compilers is much appreciated. 


Built Tested on:
- x86 g++ 11.0.1

```bash
mkdir build
cd ./build
cmake -GNinja ..
ninja
#run tests
ctest
```   
Standard `make` also works if you do not want to use ninja.

## Examples

### Set Up

```c++
/* Create an engine with configs - this example has at least 4 event loop threads running */
zab::engine e(zab::event_loop::configs{
    .threads_ = 4
});

/* Add your class(s) that inherit from engine_enabled */
your_class yc;
yc.register_engine(e);

/* Start the engine and block - your class can be bootstrapped through its initialise function */
e.start();

/* Some where else it can be stopped */
e.stop();

```

### In Class Usage

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

    //or inbuilt for_each;
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

    /* We can do some non-blocking synchronisation */

    /* mutex - for mutual exclusion */
    zab::async_mutex mtx(get_engine());

    {
        /* Acquire a scoped lock */
        auto lck = co_await mtx;
    }

    /* binary semaphore - for signalling - created in locked mode  */
    zab::async_binary_semaphore sem(get_engine(), false);

    /* release the sem */
    sem.release();

    /* aquire the sem */
    co_await sem;

    /* Lots more synchronisation primitives in the library... */

    /* File IO */
    zab::async_file file(get_engine(), "test_file.txt", async_file::Options::kRWTruncate);

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
    zab::tcp_acceptor acceptor(get_engine());
    if (acceptor_.listen(AF_INET, 8080, 10)) {

        std::optional<zab::tcp_stream> stream;
        while (stream = co_await acceptor.accept())
        {
            /* got a tcp stream! */

            /*  Read some data */
            auto data = co_await stream->read();

            /* Write some data */
            auto amount_wrote = co_await stream->write(buffer);

            /* await a graceful shutdown */
            co_await stream->shutdown();
        }
    }

}

```

### Example Application - Echo Server
See `example/echo_server.cpp` for an example implementation of an Echo Server application using the ZAB framework.

## Core Library

ZAB is event-driven and provides methods for creating generic events, signal handling, and file/network IO. ZAB also ensures thread safety for cross-thread events and communication.

### engine
The `engine` is the core object in ZAB which represents the framework. The `engine` is responsible for running the event loops, ordering events for execution, handling cross thread events, and providing both [Signal Handling](#Signal-Handling) and [Descriptor Notifications](#Descriptor-Notifications). 

The `engine` can be constructed with the following configurations:
```c++
event_loop::configs {

    enum thread_option {
        kAny,     /* Ignore threads_ var and have number of processors - 1 event loops */
        kAtLeast, /* Have std::max(threads_, number or processors - 1) event loops */
        kExact    /* Have exactly threads_ event loops */
    };

    /* How many different event loops to run */
    std::uint16_t threads_ = 1;

    /* Options to apply to threads_ */
    thread_option opt_ = kAny;

    /* Set core affinity to threads. */
    bool affinity_set_ = true;

    /* If affinity_set_, what core to start with.                                    */
    /* event loop n is bound to thread (n + affinity_offset_) % number of processors */
    std::uint16_t affinity_offset_ = 0;
};
```
The amount of threads that can be made is bounded by `2^16 - 2` (although I don't expect anyone to need anywhere near that amount).

### Ordering and Threads

An `order_t` is a strongly typed `std::size_t`. This specifies the delay to apply in the event loop in nanoseconds (0 for no delay).

A `thread_t` is a strongly typed `std::uint16_t` which is used to select a thread to be used. This can have different meaning in different contexts.  Typically `event_loop::kAnyThread` can be used to select which thread currently has the least amount of work to do. Thread bounds are not checked, so specifying an index larger than the number of threads in the `engine` is undefined behavior. 

### engine_enabled<Base>

`engine_enabled<Base>` is a helper class that uses [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) to aid in setting up you classes for easy use of the `engine` class. It also provides configurations and member function bindings to remove common boiler plate. The interface and its uses are shown in the example. *Note:* You can include as little or as much of the configurations and function bindings you choose. If a configuration is not defined in your class, the default is used.
```c++
class your_class : public engine_enabled<your_class> {
public:

    /**
     * Select the default thread to execute in when default
     * parameters are used to select the thread.
     *
     * Defaults to `event_loop::kAnyThread` which means the
     * thread with the least work.
     *
     */
    static constexpr zab::thread_t kDefaultThread = event_loop::kAnyThread;

    /**
     * This function is executed after the object is
     * "registered" with the engine. This occurs once.
     *
     * Use of `get_engine()` results in undefined behavior
     * until `initialise()` is called. Or in the case there is
     * no `initialise()` after `register_get_engine()` is called.
     *
     */
    void initialise() noexcept;

    /**
     * What thread to run the `initialise()` function in.
     *
     * Defaults to `event_loop::kAnyThread`.
     */
    static constexpr zab::thread_t kInitialiseThread = kDefaultThread;

    /**
     * This function is bound in a event cycle and executed
     * repeatably on a given cadence and thread.
     *
     */
    void main() noexcept;

    /**
     * How often to execute the `main()` function.
     *
     * Defaults to order::seconds(30).
     */
    static constexpr zab::order_t kMainCadence =  order::seconds(30)

    /**
     * What thread to run the `main()` function in.
     *
     * Defaults to `event_loop::kAnyThread`.
     */
    static constexpr zab::thread_t kMainThread = kDefaultThread;
        
};
```
`engine_enabled<Base>` provides the following `public` function:
```c++
/**
 * Registers the engine binding the correct functions and allowing
 * access to `get_engine()`
 */
bool 
register_engine(engine& _engine);

/**
 * Gets the `engine` that was registered.
 *
 * Returns `nullptr` until `register_get_engine()` is called.
 */
engine*
get_engine()
```
`engine_enabled<Base>` also provides easy access to all of [Asynchronous Primitives](#Asynchronous-Primitives).


### Descriptor Notifications
The `engine` provides an asynchronous descriptor notification service. This is used to power the [`Networking Overlay`](#Networking). You can use this service to receive notifications on any posix Descriptor that is compatible with [epoll](https://man7.org/linux/man-pages/man7/epoll.7.html). The service is essentially a wrapper for a `epoll` with the notification flags being equivalent. 

You can get access to the notification service through `descriptor_notification& engine::get_notification_handler()`. This class provides the method:
```c++
[[nodiscard]] std::optional<descriptor_waiter>
subscribe(int _fd);
``` 
Returning a valid `descriptor_waiter` on successfully subscribing the Descriptor to the notification service. The user can use the `descriptor_waiter` object to set what flags to be notified by, what timeout to use (if any), and to `co_await` for a notification. For example:
```c++
int desc = ...;
auto desc_awaiter = get_engine()->get_notification_handler().subscribe(desc);

if (desc_awaiter) {
    /* In milliseconds */
    desc_awaiter->set_timeout(1000000);

    // Equiv and interchangeable to EPOLLIN | EPOLLPRI
    desc_awaiter->set_flags(descriptor_notification::kRead | descriptor_notification::kException);

    /* If you are in an engine thread, this will return in the same thread. */
    /* If you are not in an engine thread, this will return you in an engine thread. */
    auto return_flags = co_await *desc_awaiter;

    /* zab specific flag that indicates the notification service was shut down. This most likely
     * indicates the engine has been stopped. */
    if (return_flags == kDestruction) { /* clean up */ }

    /* Both flags are not necessarily exclusive... */
    if (return_flags | descriptor_notification::kRead) { /* read data */ }

    if (return_flags | descriptor_notification::kException) { /* handle exception */}
}

```

### Signal Handling
The `engine` provides rudimentary signal handling that utilises the "self-pipe trick". Your class can access the `signal_handler` through `signal_handler& engine::get_signal_handler()` and use this to register for notification every time a signal is caught. 
```c++
bool signal_handler::handle(
    int _sig, 
    thread_t _thread, 
    handler&& _function
    ) noexcept;
```
- `_sig` The signal to catch.
- `_thread` The thread to run the notification in.
- `_function` The notification function to execute.

## Coroutine Types
Coroutine types represent different asynchronous functions types available as apart of the ZAB library. The coroutine type affects how the function can be called. The body of the coroutine code can be configured by [Promise Types](#Promise-Types). All coroutine types take a corresponding promise type as a template parameter.

ZAB currently supports: `async_function<Execution>`, `simple_future<T, Returnable>` and `reusable_future<T, Reoccurring>`. 

### async_function
A function can mark itself as a coroutine by having a return type of `async_function`. 

The promise type for an `async_function` must satisfy the `Execution` concept. The default promise type is the [`execution_promise`](#execution_promise).

```c++
async_function<> 
foo() noexcept 
{
   /* async behaviour goes here... */
}
```
### simple_future<T>
A function can mark itself as a coroutine by having a return type of `simple_future<T, Promise>`. The purpose of a simple_future is to provide a method for asynchronous functions to have a return type. Since the `simple_future<T>` may exhibit asynchronous behaviour the return type must be accessed through `co_await`. A `simple_future<T>` will wake up the waiting coroutine and return a value to it be using the `co_return` keyword. 

The promise type for an `simple_future<T>` must satisfy the `Returnable` concept. The default promise type is the [`simple_promise<T>`](#simple_promise<T>).


```c++
template <typename T = void>
simple_future<T> 
future_value() noexcept 
{
    /* async behaviour goes here... */
    co_return T{};
}

/*
    template <typename T>
    using guaranteed_future = simple_future<promise_always_resolves<T>,  simple_promise<promise_always_resolves<T>>>;
*/
template <typename T = void>
guaranteed_future<T> 
guaranteed_future_value() noexcept 
{
    /* async behaviour goes here... */
    co_return T{};
}

async_function<> 
foo() noexcept 
{
    /* no return type (just awaitable)- void */
    co_await future_value<>();

    /* Simple return type - bool */
    bool predicate = co_await future_value<bool>();

    /* All others return an std::optional<T> - std::size_t */
    std::optional<std::size_t> value = co_await future_value<std::size_t>();

    /* Turn off optional by guaranteeing it will resolve - std::size_t */
    std::size_t value = co_await guaranteed_future_value<std::size_t>();
}
```

The return type is taken from `Promise::ReturnType`.

_Note: A `simple_future` may return to you in a different thread than was entered (if the application explitelty does the swap in the body). [Proxies](#proxy) can be used for simple thread coordination._ 


### ReusuableFuture<T> 
A function can mark itself as a coroutine by having a return type of `reusable_future<T, Promise>`. A reusable future is essentially `simple_future<T, Promise>` that can be `co_await`'ed multiple times. The return type is always wrapped in an `std::optional` since the `reusable_future<T, Promise>` may have finished or hit an error. A user can use the `bool complete()` to distinguish the two. A `reusable_future<T, Promise>` can be viewed as a generator that may exhibit asynchronous behavior. A `reusable_future<T>` will wake up the waiting coroutine and return a value to when the `Promise` uses either the `co_yield` or `co_return` keywords. `co_yield` will return a value, but allow a the `reusable_future<T, Promise>` to be `co_await`ed again. `co_return` will complete the coroutine and any further `co_await` attempts on the `reusable_future<T, Promise>` will fail.

The difference in use cases between using a `simple_future<T. Promise>` many times and having a single `reusable_future<T, Promise>` is that a `reusable_future<T, Promise>` can store state about its context between value returns. 

The promise type for an `reusable_future<T, Promise>` must satisfy the `Reoccurring` concept. The default promise type is the [`reusable_promise<T>`](#reusable_promise<T>).

An example of `reusable_future<T>` using the [`yield`](#yield) Asynchronous Primitive is:
```c++

simple_future<std::string> 
attempt_foo() noexcept;

reusable_future<std::size_t>
your_class::exponential_delay(std::size_t _max) noexcept 
{
    std::size_t delay = 2;

    while (delay < _max)
    {

        co_await yield(order_t(order::now() + order::seconds(delay)));

        delay *= 2;

        co_yield delay;
    }

    /* dealyed to long... */
    co_return std::nullopt_t;
}


async_function<> 
your_class::run() noexcept 
{
    while (auto delay_func = exponential_delay(256); !delay_func.complete())
    {

        std::optional<std::string> foo = co_await attempt_foo();

        if (foo)
        {

            std::cout << "Got foo: " << *foo << "\n";
            delay_func.force_completion();
        }
        else
        {

            std::optional<std::size_t> delay_length = co_await delay_func;

            if (delay_length) { std::cout << "Delayed for: " << *delay_length << "\n"; }
            else
            {

                std::cout << "Timed out\n";
            }
        }
    }
}
```

_Hint: A `reusable_future<std::variant<T...>>` can be used to implement state-machines that have differing return types._

## Promise Types

Promise types are used to configure the functionality of coroutine body and to define how return/yield results make it to the underlying [Coroutine Type](#Coroutine-Types).

### execution_promise

This is the most basic promise. It simply begins execution on creation, and cleans itself up after completion. `execution_promise` satisfies the `Execution` concept.

The only coroutine key word available is `co_return (void)` and `co_await (AwaitableProxy)`.

### defer_block_promise

A `defer_block_promise` is an extension of `execution_promise` that allows deferred execution of some function/s.  `defer_block_promise` satisfies the `Execution` concept.

The coroutine key words available are `co_return (void)`, `co_yield (defer_block)`, `co_yeild (purge_block)`. The coroutine can also suspend using `co_await (AwaitableProxy)`.

The `defer_block_promise` allows the user to save a/many callable/s of signature `void(void)` which will executed during the final suspend phase of the coroutine (after `co_return` is called). If multiple callable's are saved, they will be executed in reverse order of deferment.

A code block can be deferred by using the `co_yield (defer_block)` operator. All deferred code blocks can be removed through the `co_yeild (purge_block)` operator. These operators do not actually suspend the coroutine.

Access to local variables in the functions scope is questionably undefined behavior. For example, capturing from the function scope by reference. The deconstuctors will have been called but the memory would have not been de-allocated. Accessing local variables with trivial / no de-constructors (file descriptors) is not a memory violation. Experimentation indicates correct behavior although optimizations present or future could easier mess it up. Accessing local variables with non-trivial de-constructors is undefined behavior. 

Example:
```c++
async_function<defer_block_promise> 
defer_example() noexcept 
{
    co_yield defer_block(
        []
        () noexcept
            {
                std::cout << "This will not run :(\n";
            }
        );

    /* Remove all defer blocks... */
    co_yield purge_block{};

    co_yield defer_block(
        []
        () noexcept
            {
                std::cout << "This will run 2nd :)\n";
            }
        );

    co_yield defer_block(
        []
        () noexcept
            {
                std::cout << "This will run  1st :)\n";
            }
        );

    co_return;
    /* The defer blocks will run after this return... */
}
```
### simple_promise<T>
A `simple_promise<T>` is the simplest "returning" promise. The internal state maintains an `optional<T>` which is set by the coroutine body. This allows a [coroutine Type](#Coroutine-Types) to pull out the return type and provide it to the caller. `simple_promise<T>` satisfies the `Returnable` concept.

The coroutine key words available are `co_return (ARG)` where T is constructible from `ARG`, `co_return (std::nullopt_t)` in the case the promise cannot be fulfilled and `co_await (AwaitableProxy)` for suspension.

### visitor_promise<T>

A `visitor_promise<T>` is an extension of `simple_promise<T>` that allows deferred execution of some function/s to "visit" or "preview" the returned type. `visitor_promise<T>` satisfies the `Returnable` concept.  

The `visitor_promise<T>` allows the user to save a/many callable/s of signature `void(T&)` which will executed during the final suspend phase of the coroutine (after `co_return` is called). If multiple callable's are saved, they will be executed in reverse order of deferment.

A code block can be deferred by using the `co_yield (VistorBlock<T>)` operator. All deferred code blocks can be removed through the `co_yeild (purge_block)` operator. These operators do not actually suspend the coroutine.

Example:
```c++
/*
 * template<typename T = void>
 * using visitor_future = simple_future<T, visitor_promise<T>>;
 * 
 * This function would return 3.
 */
visitor_future<std::size_t> 
always_odd() noexcept 
{
    co_yield defer_block(
        [this, code_]
        (std::size_t& _size) noexcept
            {
                std::cout << "Tried to return " << _size << "\n";
                /* Make sure the return value is odd... */
                _size |= 1;
            }
        );


    co_return 2;
    /* The defer blocks will run after this return... */
}

```
### reusable_promise<T>

A `reusable_promise<T>` allows a coroutine to return values to the caller multiple times. `reusable_promise<T>` satisfies the `Reoccurring` concept.   

The coroutine key words available are `co_yield (ARG)`, `co_return (ARG)` where T is constructible from `ARG`, and `co_yield (std::nullopt_t)`, `co_return (std::nullopt_t)` in the case the promise cannot be fulfilled.


## Asynchronous Primitives
Classes that inherit from `engine_enabled<your_class>` have four main asynchronous primitives: `yield(...)`, `pause(...)`, `wait_for(...)` and `proxy(...)`. The first three have equivalents that can be used outside of a `engine_enabled<your_class>` but require an additional `engine*` parameter and have no default parameters.  

### yield
Defer computation of the current function by passing control back to the event loop. _This defers the execution of the function without blocking the event loop_.
```c++
auto yield(
        order_t _order = Base::now(),
        thread_t _thread  = Base::kDefaultThread
        ) noexcept;

auto yield(
        thread_t _thread
        ) noexcept;
```
#### Parameters
- `_order` specifies when to resume the function. This is useful for imposing delays or breaking up expensive functions in order not to block the event loop. 
- `_thread` specifies what thread to return into. This can be used to jump execution of your function into different threads (to access shared state) or to specify that you don't care what thread this executes in `event_loop::kAnyThread`. 

Example Usage:
```c++
async_function 
delay() noexcept 
{ 
    /* wait 10 seconds then resume in thread 2. */
    co_await yield(
        order::now() + order::seconds(10),
        thread_t{2}
        );
}
```

### pause
pause the execution of the current coroutine until it is manually resumed. Useful for deferring for an unspecified or unknown amount of time. For example, waiting for the user to do something. 

```c++
struct pause_pack {
    std::coroutine_handle<> handle_ = nullptr;
    thread_t                thread_ = kDefaultThread;
    std::uintptr_t               data_   = 0;
};

template <typename Functor>
        requires(std::is_nothrow_invocable_v<Functor, pause_pack*>)
auto 
pause(Functor&& _pause) noexcept;

void 
unpause(
        pause_pack& _pause,
        order_t _order = Base::now()
    );
```
A `pause_pack` has a non-owning reference to the coroutines state and also provides two options to set for resumption: The data to return and the thread to resume in.  

After the coroutine has been paused `thread_` and `data_` can be set to the desired state and then the coroutine can be resumed using `unpause(...)`. This resumption is thread-safe so may be called in any thread, but the coroutine will be resumed in the thread specified by  `thread_`.

`pause(...)` will give the Functor a non-owner ptr to the pause_pack. This ptr is valid until `unpause` has been called with it. 


Example Usage:
```c++
async_function 
your_class::do_pause() noexcept 
{ 
    pause_pack return_pp = co_await pause(
        []
        (pause_pack* _pp) noexcept
            {
                /* resume the coroutine in thread 1*/
                _pp->thread_ = thread_t{1};
                /* Set the data to 42 */
                 _pp->data_ = 42;
                /* wait 10 seconds and resume */
                unpause(_pp, order::now() + order::seconds(10));  
            }
        );

    /* now executing in thread 1 */
    assert(return_pp.data_ == 42);
    assert(return_pp.thread_.thread_ == 1);
}

```
### wait_for
Creates a `simple_future` out of a series of `simple_future`s or `reusable_future`s. This function will run all passed `simple_future`s in parallel and return the results after all have complete. The dynamic overload (2) requires all `simple_future`s are of the same type.
```c++
/* (1) static number of arguments known at compile time. */
template <typename... Promises>
guaranteed_future<
    typename details::ExtractPromiseTypes<Promises...>::types
> 
wait_for(Promises&&... _args);

/* (2) dynamic number of arguments known at run time. */
template <typename T>
guaranteed_future<
    std::vector<typename simple_future<T>::return_value>
> 
wait_for(std::vector<simple_future<T>>&& _args)
```
##### Parameters 

- `_args`   The `simple_future`s to run.


(1) The return type of `co_await wait_for(...)` is a tuple filled with the concatenation of all the `simple_future::return_value`s for every passed `simple_future` or `reusable_future`. 

(2) The return type of `co_await wait_for(...)` is a vector filled with the `simple_future::return_value`. The vector will be the same size of `_args`.  

If a void specialisation of `simple_future` is given then an empty `details::promise_void` struct is used as `simple_future::return_value` instead.

Example Usage:

```c++

template <typename T = void>
simple_future<T> future_value() noexcept 
{
   /* async behaviour goes here... */
   co_return T{};
}

async_function 
your_class::foo() noexcept 
{ 
    std::tuple<
        details::promise_void,
        bool,
        std::optional<std::size_t>
    > result = co_await wait_for(
        get_engine(),
        future_value<>(),
        future_value<bool>(),
        future_value<std::size_t>() 
    );
}
```
Unlike other `simple_futures<T>`, `wait_for` will always return the caller in the same thread. Coroutine arguments are thread safe, so can return in any thread they choose.

### proxy
A `proxy` is a way for classes to expose asynchronous functions through their public interface that have thread based pre and post conditions. A `proxy` to a member function places the path of execution into the indicated thread before calling/re-entering the member function then returning the path of execution to the calling thread on exiting. In the case of `simple_futures<T>` and `reusable_futures<T>` this switch occurs on `co_await`. If you are already in the correct thread, the proxy code no-ops. 

The gerneral format for a proxy call is:
```c++
RET<> proxy(
    RET (your_class::*)(ARGS1...),
    thread_t,
    ARGS2...
    );

```
Where `ARGS1` is constructible from `ARGS2`. _NOTE:_ `ARGS2` is always taken by copy because using parameter packs with universal references and stacked coroutines almost always ends in undefined behavior. If your member function takes references, it will be references to the proxy created arguments, not the caller arguments. For `simple_futures<T>` and `reusable_futures<T>` this is recommended, as the proxy created arguments are guarantied to live longer then the member coroutine, and reduces an extra copy.  For `async_function`, since there is no waiting, the copied parameters will live until the first suspension.

An example of using proxy with a `simple_future<T>` is:
```c++

...

public:

    simple_future<bool> 
    do_foo(const std::vector<std::uint8_t>& _p1, int _p2)
    {
        return proxy(
            &your_class::Foo, /* Proxies the Foo function */
            thread_t{2}      /* Foo will execute in thread 2*/
            _p1,
            _p2
            );
    }

private:

     simple_future<bool> 
     foo(std::vector<std::uint8_t>& _f1, int _f2)
     {
        /* Here _f1 is a reference to the proxy copied _p1         */
        /* This is why we can take by reference, even though DoFoo */
        /* takes by const reference...                             */
        ...
     }

...

async_function 
user_of_foo()
{
    /* We are running in thread 1 */

    /* The proxy will execute this in thread 2 */
    auto result = co_await DoFoo({1, 2, 3}, 2);

    /* We will return in thread  1*/
}

```

A `proxy` is the only asynchronous primitive with dedicated thread coordination. It will always return into the calling thread, and always call the member function in the specified thread. It can be used for basic [Asynchronous Synchronisation](#Asynchronous_Synchronisation) but is far more general then the constructs provided in there. A `proxy` will remove any benefits of parallelism for the entire member function given (which you may want). For more multi-thread friendly coordination, using the [Asynchronous Synchronisation](#Asynchronous_Synchronisation) constructs will give better results.


## Asynchronous Synchronisation
ZAB provides synconisation mechanisms similar to those provided by the [C++ standard thread support library](https://en.cppreference.com/w/cpp/thread). The main difference is that these synchronisation mechanisms are _non-blocking_ in terms of the event loop. This allows mutual exclusion and thread co-ordination without interfering with the event loops. This also allows the same thread to "wait" or "co-ordinate" with itself. For example, the same thread may have different coroutines all blocking on the same `async_mutex`.  

### pause_token
A `pause_token` is the only synchronisation class in ZAB that has no standard library equivalent. It is used for pauseing and unpausing coroutines in a thread safe manner. All `pause_token` functions except the deconstructor are thread safe. The `pause_token` is constructed in a paused state.

If paused, coroutines that `co_await` the `pause_token` will have their execution suspended. Otherwise, `co_await` no-ops.

If the `pause_token` changes from a paused state to an unpaused state, all coroutines that had their execution suspended will be resumed in the _same_ thread they were suspended in. 

The `pause_token` is re-usable and can be switched between paused and unpaused states. There is no maximum number of coroutines that can be suspended. Although, unpausing is linear in the number of suspended coroutines. 

Example Usage:

```c++

async_function<> 
your_class::worker(thread_t _thread, pause_token& _pt)
{   
    /* Move into our thread */
    co_await yield(order_t{order::now()}, _thread);

    /* If it was already unpaused this will just no-op */
    co_await _pt;

    std::cout << "At work " << _thread << "\n";
}


async_function<> 
your_class::pause_example(pause_token& _pt)
{
    const auto threads = get_engine()->get_event_loop().number_of_workers();
    pause_token pt(get_engine());

    for (std::uint16_t t = 0; t < threads; ++t) {
        worker(thread_t{t}, pt);
    }

    /* wait 5 second... */
    co_await yield(order_t{order::now() + order::seconds(5)});

    std::cout << "Putting to work\n";
    pt.unpause();
}

```

### async_counting_semaphore
A `async_counting_semaphore` is the equivalent of the [std::counting_semaphore](https://en.cppreference.com/w/cpp/thread/counting_semaphore).

> A counting_semaphore is a lightweight synchronization primitive that can control access to a shared resource. Unlike a std::mutex, a counting_semaphore allows more than one concurrent access to the same resource, for at least LeastMaxValue concurrent accessors. The program is ill-formed if LeastMaxValue is negative.

> A counting_semaphore contains an internal counter initialized by the constructor. This counter is decremented by calls to _operator co_await_, and is incremented by calls to release(). When the counter is zero, operator co_await suspends until the counter is incremented, but try_acquire() does not block; 

`try_acquire_for()` and `try_acquire_until()` are not included in this interface.

```c++

guaranteed_future<size_t> 
your_class::to_wake_up();

async_function<> 
your_class::do_work(thread_t _thread, async_counting_semaphore<>& _sem)
{
    /* Move into our thread */
    co_await yield(_thread);

    /* Consumer loop wont block as it suspends... */
    while (true)
    {

        /* Process work as it comes in... */
        co_await _sem;

        /* Do some work... */
    }
}

async_function<> 
your_class::queue_work()
{
    /* Move into thread 0 */
    co_await yield(thread_t{0})

    async_counting_semaphore<> sem(get_engine(), 0);

    /* Create worker [logical] threads 1 - n */
    const auto threads = get_engine()->get_event_loop().number_of_workers();
    assert(t > 1);
    for (std::uint16_t t = 1; t < threads; ++t)
    {
        do_work(thread_t{t}, sem);
    }

    /* Producer loop wont block as it suspends... */
    while (true)
    {
        /* Get work from somewhere */
        auto to_wake_up = co_await to_wake_up();

        /* notify to wake up threads */
        sem.release(to_wake_up);
    }
}

```

### async_binary_semaphore
A `async_binary_semaphore` is the equivalent of the [std::binary_semaphore](https://en.cppreference.com/w/cpp/thread/counting_semaphore). 

> binary_semaphore is an alias for specialization of std::counting_semaphore with LeastMaxValue being 1. Implementations may implement binary_semaphore more efficiently than the default implementation of std::counting_semaphore.

Same description and basic interface as `async_counting_semaphore`. 

_Note:_
>Semaphores are also often used for the semantics of signalling/notifying rather than mutual exclusion, by initializing the semaphore with 0 and thus blocking the receiver(s) that try to acquire(), until the notifier "signals" by invoking release(n).

For mutual exclusion see [async_mutex](#async_mutex).

Example Usage:

```c++

simple_future<void> your_class::get_work();

async_function<> 
your_class::do_work(thread_t _thread, async_counting_semaphore<1>& _sem)
{   
    /* Move into our thread */
    co_await yield(_thread);

    /* Consumer loop wont block as it suspends... */
    while (true) {

        /* Process work as it comes in... */
        co_await _sem;

        /* Do some work... */
    }
}

async_function<> 
your_class::queue_work()
{
    /* Move into thread 0 */
    co_await yield(thread_t{0})

    async_counting_semaphore<1> sem(get_engine(), false);

    /* Create worker [logical] threads 1 - n */
    const auto threads = get_engine()->get_event_loop().number_of_workers();
    assert(t > 1);
    for (std::uint16_t t = 1; t < threads; ++t) {
        do_work(thread_t{t}, sem, work_orders);
    }

    /* Producer loop wont block as it suspends... */
    while (true) {

        /* Get work from somewhere */
        co_await get_work();

        /* notify a thread */
        sem.release();
    }
}

```

### async_mutex
The `async_mutex` is a merger of a [`std::mutex`](https://en.cppreference.com/w/cpp/thread/mutex) and a [`std::lock_guard`](https://en.cppreference.com/w/cpp/thread/lock_guard). 

> The mutex class is a synchronization primitive that can be used to protect shared data from being simultaneously accessed by multiple _coroutines_.

> mutex offers exclusive, non-recursive ownership semantics: 

With `async_mutex`, `non-recursive` and locking multiple times in the same thread are different. Multiple independent coroutines in the same thread can attempt to acquire the `async_mutex`. Although, if a coroutine acquires the lock, that same coroutine or any coroutines it is currently `co_await`ing (to any depth) cannot attempt to acquire the `async_mutex`. This will lead to a deadlock.  

> - A calling _coroutine_ owns a mutex from the time that it successfully calls either _operator co_await_ or try_lock until it calls unlock.
> -When a _coroutine_ owns a mutex, all other _coroutines_ will block (for calls to _operator co_await_) or receive a false return value (for try_lock) if they attempt to claim ownership of the mutex.
> - A calling _coroutine or dependents on the coroutine_ must not own the mutex prior to calling _operator co_await_ or try_lock. 

> The behavior of a program is undefined if a mutex is destroyed while still owned by any threads, or a thread terminates while owning a mutex. 

> mutex is neither copyable nor movable. 

With regards to the return value of `operator co_await`, `async_lock_guard`:

> The class lock_guard is a mutex wrapper that provides a convenient RAII-style mechanism for owning a mutex for the duration of a scoped block. 

> When a lock_guard object is created, it attempts to take ownership of the mutex it is given. When control leaves the scope in which the lock_guard object was created, the lock_guard is destructed and the mutex is released.

> The lock_guard class is non-copyable. 

Example Usage:
```c++

struct ProectedObject {
    async_mutex mtx_;
    int needs_protecting_;
}

async_function<> 
set_value(ProectedObject _object, int _value)
{   
    /* suspend until we can get the lock */
    async_lock_guard lock = co_await _object.mtx_;

    /* We can safely edit the value      */
    _object.needs_protecting_ = _value;

    /* We can safely exhibit asynchronous behavior without deadlocks */
    co_await yield();

    /* lock will be released by RAII                           */
    /* Note: resumption of blocked coroutines is not done here */
    /* Resumption is yielded and performed later...            */
}
```

### async_latch
An `async_latch` is the equivalent of the [std::latch](https://en.cppreference.com/w/cpp/thread/latch).

> The latch class is a downward counter of type std::ptrdiff_t which can be used to synchronize threads. The value of the counter is initialized on creation. _Coroutines will be suspended_ on the latch until the counter is decremented to zero. There is no possibility to increase or reset the counter, which makes the latch a single-use barrier. 

> Concurrent invocations of the member functions of latch, except for the destructor, do not introduce data races. 

[`wait_for`](#wait_for) is an good example for where latches are useful. In particular the ability to use within the same thread multiple times.

Example Usage:
```c++  

async_function<> 
your_class::do_work(thread_t _thread, async_latch& _latch)
{   
    /* Move into our thread */
    co_await yield(_thread);

    /* Do some pre-work or something... */

    co_await _latch.arrive_and_wait();

    /* Do some work... */
}

async_function<> 
your_class::spawn_threads()
{
    const auto threads = get_engine()->get_event_loop().number_of_workers();
    async_latch latch(get_engine(), threads + 1);

    /* Create worker [logical] threads 0 - n */
    for (std::uint16_t t = 0; t < threads; ++t) {
        do_work(thread_t{t}, latch);
    }

    co_await _latch.arrive_and_wait();

    /* If we get here, all coroutines are in the correct thread  */
    /* and completed their pre-work */
}

```

### async_barrier
A `async_barrier` is the equivalent of the [std::barrier](https://en.cppreference.com/w/cpp/thread/barrier).

>The class template std::barrier provides a thread-coordination mechanism that allows at most an expected number of threads to _suspend_ until the expected number of threads arrive at the barrier. Unlike std::latch, barriers are reusable: once the arriving threads are _resumed_ from a barrier phase's synchronization point, the same barrier can be reused. 
>
> A barrier object's lifetime consists of a sequence of barrier phases. Each phase defines a phase synchronization point. Threads that arrive at the barrier during the phase can _suspend_ on the phase synchronization point by calling wait, and will be _resumed_ when the phase completion step is run. 

See [Unbounded use of async_barrier](#Unbounded-use-of-async_barrier) for some oddities around phases and phase sequencing. This can occur more evidently then the thread based `std::barrier`, 

> A barrier phase consists following steps: 
>   + The expected count is decremented by each call to arrive[ , arrive_and_wait] or arrive_and_drop.
>   + When the expected count reaches zero, the phase completion step is run. The completion step invokes the completion function object, and _resumes all suspended frames_ on the phase synchronization point. The end of the completion step strongly happens-before the returns from all calls that were _suspended_ by the completion step.
>  + When the completion step finishes, the expected count is reset to the value specified _(barrier count)_ at construction less the number of calls to arrive_and_drop since, and the next barrier phase begins. 
> Concurrent invocations of the member functions of barrier, except for the destructor, do not introduce data races.

The implementation differs (so far) in terms of `arrival_token` and the use of `arrival_token arrive()`. 
+ You cannot specifiy an amount to decrement for `arrive()`
+ The lifetime of the `arrival_token` must be greater then the phase it arrived out (see [Unbounded use of async_barrier](#Unbounded-use-of-async_barrier))
+ The `arrival_token` can live past its own phase and the next phase without causing undefined behavior
+ There is no corresponding `wait(arrival_token&&)` function and suspension can be achieved by `co_await`ing the `arrival_token`. 

With `async_barrier` the completion function is allowed to be asynchronous. If the completion function satisfies `NoThrowAwaitable` the barrier will `co_await` the function before continuing. 

Example Usage:
```c++


simple_future<> 
your_class::do_map();

using ExampleBarrier = async_barrier<std::function<async_function<>()>>;

async_function<> 
your_class::do_reduce(thread_t _thread, std::shared_ptr<ExampleBarrier> _barier, std::size_t _iterations)
{   
    /* Move into our thread */
    co_await yield(_thread);

    /* Consumer loop wont block as it suspends... */
    while (_iterations) {

        /* wait for all threads to synchronize... */
        co_await _barier->arrive_and_wait();

        /* Do some work... */

        --_iterations;
    }

    /* Remove us from the barrier */
    _barier->arrive_and_drop();
}


async_function<> 
your_class::map_reduce(std::size_t _parelle_count)
{
    /* Move into thread 0 */
    co_await yield(thread_t{0})

    std::vector<Work*> work_orders;

    /* A barrier with _parelle_count threads at a time... */
    auto barrier = std::make_shared<ExampleBarrier>(
        engine_,
        _parelle_count,
        [&work_orders]()
            {   
                /* now all threads are waiting    */
                /* lets do our unsafe map work    */
                co_await do_map();

                /* After this all threads will    */
                /* start reducing again           */
                co_return;

            } -> simple_future<>,

            thread_t{0} /* Map needs to happen in thread 0 */
        );
    
    /* Create worker [logical] threads 0 - _parelle_count */
    for (std::uint16_t t = 0; t < _parelle_count; ++t) {
        do_reduce(thread_t{t}, barrier, t + 10);
    }
}


```


#### Unbounded use of async_barrier
[std::barrier](https://en.cppreference.com/w/cpp/thread/barrier) (although `std::barrier` does not) mentions the concept of "thread binding" which requires that the same threads be used every time, and therefore the number of threads using the `std::barrier` must remain constant (ignoring `arrive_and_drop`) and must be equal to the `barrier count`.

`async_barrier` can not only handle a arbitrary amounts of different threads, the same thread can arrive at the barrier multiple times by suspending corountines. As a result of this, for a barrier phase there might actually be more then the `barrier count` frames suspended. This can lead to a phonenum for queued phase waiting. This happens more evidently as `async_barrier` will never block. So we must suspend the co-routines instead of blocking until they can successfully become part of a phase (decrement the counter).  

In terms of function guaranties, the "current phase" is not the currently executing phase, but the phase that its arrival frame will form part of. This is important when considering when a function strongly happens-before the start of the phase completion step for the _current phase_.  

For example, a barrier that has a `barrier count` of 5, there might be 15 frames currently suspended at the barrier. This would be very rare and would require some odd thread scheduling by the OS (priority inversion limiting the control thread) or user code blocking the control thread. Essentially, the exhibited behavior would be as expected: as if the 10 extra frames have not suspended yet but are about to. The control thread calls the completion function then will resume the first 5 frames. If the amount of left over frames is greater then `barrier count` a new control thread is picked to repeat. The application would see: `[completion_function() -> process 5 -> completion_function() - > process 5 -> completion_function() -> process 5]` in rapid succession.

The use of `arrive_and_drop()` seemingly adheres to the standard, but in the presence queued phase waiting might behave oddly according to your application. According to the published `std::barrier`, `arrive_and_drop()` does not block. So this function will never block _even_ if the drop in count does not count towards the next executing phase. In the above example, `arrive_and_drop()` could be called before the first phase is complete, but it applies to the last phase queued. This means that `arrive_and_drop()` returns, but the phase it applies to has not yet started. In this case the application would see: `[completion_function() -> process 5 -> completion_function() - > process 5 -> completion_function() -> process 5 -> decrement barrier count]` in rapid succession but `arrive_and_drop()` may return before this occurs.


## Overlays
Overlays represent some functionality built by using the above asynchronous primitives that represent complex operations. Zab includes some basic overlays to give the expected functionality of an event loop framework.

### File IO

The file IO overlay provides simple asynchronous read/write operations to the user. An `async_file` is essentially just a wrapper for `fopen`, `fread`, and `fwrite`. 

Unlike other event loops, file io is not designated to its own thread. This is because, by default, zab utilises all physical cores on the running device already. Instead, all file io is designated to the last event loop maintained by the `engine`. For example, if the `engine` is running 4 threads, all file io is done in thread 3. This is a common theme for zab, where all "system" or "overlay" functionality that does not rely on a specific thread, will be placed in the last thread. 

See `includes/zab/file_io_overlay.hpp` for more comprehensive documentation.

`async_file` usage in an `engine_enabled` class:
```c++
async_function<> 
your_class::file_io_example()
{
    /* Open with read, write, and truncate */
    async_file file(get_engine(), "test_file.txt", async_file::Options::kRWTruncate);

    std::vector<char> buffer(42, 42);

    /* Write data to file - return in the io thread  */
    bool success = co_await file.write_to_file(file.io_thread(), buffer);

    /* Note: returning and entering file functions to/from the io thread */
    /* will result in sliglty faster code (slightly...)                  */

    /* Note 2: async_file is not thread-safe. Only use in 1 thread at a time. */

    if (success)
    {

        /* Re position the file ptr */
        success = file.position(0);

        if (success)
        {

            /* Read the data and return to our default thread    */
            /* We are entering this function from the io thread. */
            std::optional<std::vector<char>> data = co_await file.read_file(default_thread());

            if (data && *data == buffer) { std::cout << "File io was successful.": }
        }
    }
}
```

### Networking

The zab networking overlay is intended to provide low level posix network control. The goal here was not to create a higher level of abstraction for networking, but to provide a standard non-blocking asynchronous model for common network activities. At the moment, TCP/IP is only supported. More complex overlays can be built on-top to provide more structured or simpler networking functionality (for example: a HTTP server). The overlay comprises of three main networking classes: `tcp_acceptor`, `tcp_connector` and `tcp_stream`.

See `includes/zab/network_overlay.hpp` or `includes/zab/tcp_stream.hpp` for more comprehensive documentation.

#### `TCPAccptor`

The `TCPAccptor` represents the traditional server side of a connection. It will listen for connections on an inbound port. Deconstruction or moving the `tcp_connector` while in use results in undefined behavior. Having more then one waiter also results in undefined behavior.

Example Usage:
```c++
async_function<>
YouClass::run_acceptor()
{
    tcp_acceptor acceptor(get_engine());
    // listen for IPv4 on port 8080 with a maximum backlog of 10
    // See bind(2) and listen(2) for more details...
    if (acceptor.listen(AF_INET, 8080, 10))
    {
        while (true)
        {
            std::optional<tcp_stream> stream_opt = co_await acceptor.accept();

            if (stream_opt) 
            { 
                std::cout << "Got a connection\n"; 
            }
            else
            {
                std::cout << "Got a error" << acceptor.last_error() << "\n";
            }
        }
    }
}

```

#### `tcp_connector`
The `tcp_connector` represents the traditional client side of a connection. It will attempt to connect to a listening host. Deconstruction or moving the `tcp_connector` while in use results in undefined behavior. Having more then one waiter also results in undefined behavior.

Example Usage:
```c++
async_function<>
YouClass::run_connector()
{
    tcp_connector connector(get_engine());

    /* Fill out posix struct for host details... */
    struct addrinfo  hints;
    struct addrinfo* addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    auto success      = getaddrinfo("localhost", "8080", &hints, &addr);

    if (success)
    {
        for (auto current = addr; (bool) addr; addr = addr->ai_next)
        {

            std::optional<tcp_stream> stream_opt = co_await connector_.connect(
                (struct sockaddr_storage*) current->ai_addr,
                sizeof(*current->ai_addr));

            if (stream_opt) { 
                std::cout << "Got a connection\n"; 
                break;
            }
            else
            {
                std::cout << "Got a error" << connector_.last_error() << "\n";
            }
        }

        freeaddrinfo(addr);
    }
}
```

#### `tcp_stream`
The `tcp_stream` represents the a duplex network stream for writing and reading data. `Read`, `Shutdown` and `CancelRead` on not thread safe. `Write` and related functions are thread safe. Deconstruction or moving the `tcp_stream` while in use results in undefined behavior. A user should make sure to ensure to `Shutdown` and wait for any readers to leave the function before deconstructing the object.

Example Usage:
```c++
async_function<>
YouClass::run_stream(tcp_stream&& _stream)
{
    /* Save locally so doesnt go out of scope when we suspend */
    tcp_stream stream(std::move(_stream));

    auto data_to_send = "hello world";
    std::size_t written = co_await stream.Write(data_to_send);

    if (written == ::strlen(data_to_send)) {

        /* Get 1 byte back as confirmation */
        auto buffer_opt = co_await stream.Read(1);

        /* Read can return less then requested but not more... */
        if (buffer_opt && buffer_opt->size()) {
            std::cout << "Received: " << buffer_opt.at(0) << "\n";
        }
    }

    /* Ensure we shut it down! (flushes all writes and wakes any readers) */
    co_await stream.ShutDown();

    /* RAII will clean up for us! */
}

```

