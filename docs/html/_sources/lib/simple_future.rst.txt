.. _simple_future:

=============
simple_future
=============

A function can mark itself as a coroutine by having a return type of ``simple_future<T, Promise>``. The purpose of a simple_future is to provide a method for asynchronous functions to have a return type. Since the ``simple_future<T>`` may exhibit asynchronous behaviour the return type must be accessed through ``co_await``. A ``simple_future<T>`` will wake up the waiting coroutine and return a value to it be using the ``co_return`` keyword. 

The promise type for an ``simple_future<T>`` must satisfy the ``Returnable`` concept. The default promise type is the :ref:`simple_promise`.

.. code-block:: c++
    :caption: Example
    
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

The return type is taken from `Promise::ReturnType`.

`Note:` A ``simple_future`` may return to you in a different thread than was entered (if the application explitelty does the swap in the body). :ref:`proxy` can be used for simple thread coordination.


.. doxygenclass:: zab::simple_future
   :members:
   :protected-members:
   :undoc-members: