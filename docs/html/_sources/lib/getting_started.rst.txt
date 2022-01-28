.. _getting_started:

==============
Geting Started
==============


Building
********

Portability has not been the primary focus in the creation of **ZAB**. Any and all help with porting and verifying ZAB works across different platforms and compilers is much appreciated. 

Built Tested on:

* x86 g++ 11.0.1

The dependancy liburing has kernal version requirments and the build has been tested on kernal version 5.11. 

.. code-block:: bash
    :caption: Example

    mkdir build
    cd ./build
    cmake -GNinja ..
    ninja
    #run tests
    ctest

Standard `make` also works if you do not want to use ninja.


Adding to your project
**********************

Using cmake you can just add the project as a subdirector (git submole works well.):

.. code-block:: cmake
    :caption: cmake file

    add_subdirectory(zab)

    # Add zab and liburing includes
    target_include_directories(your_project PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zab/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/zab/liburing/src/include
    )

    # Link to the libaries required
    target_link_libraries(
        your_project PUBLIC
         zab -lpthread -latomic uring
    )

    # Add path to liburing.a
    target_link_directories(
        your_project PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/zab/liburing/src
    )

Example Usage
*************
.. code-block:: c++
    :caption: Usage in you engine_enable class

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

        /* aquire the sem */
        co_await sem;

        /* Lots more synchronisation primitives in the library... */

        /* File IO */
        zab::async_file<char> file(engine_);
        
        auto success = co_await file.open("test_file.txt", async_file::Options::kRWTruncate);

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
        if (acceptor_.listen(AF_INET, 8080, 10)) {
            
            co_await zab::for_each(
                acceptor_.get_accepter(),
                [&](auto&& _stream) noexcept -> zab::for_ctl
                {
                    if (_stream)
                    {
                        /*  Read some data */
                        auto data = co_await stream->read(42);

                        /* Write some data */
                        auto amount_wrote = co_await stream->write(buffer);

                        /* await a graceful shutdown */
                        co_await stream->shutdown();

                        return zab::for_ctl::kContinue;
                    }
                    else
                    {
                        return zab::for_ctl::kBreak;
                    }
                });
        }

    }



