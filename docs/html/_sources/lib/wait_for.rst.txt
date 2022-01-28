.. _wait_for:

========
wait_for
========

Creates a ``simple_future`` out of a series of ``simple_future`` s or ``reusable_future`` s. This function will run all passed ``simple_future`` s in parallel and return the results after all have complete. The dynamic overload requires all ``simple_future`` s are of the same type.

.. code-block:: c++
    :caption: Example

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
            engine_,
            future_value<>(),
            future_value<bool>(),
            future_value<std::size_t>() 
        );
    }

Unlike other ``simple_futures<T>``, ``wait_for`` will always return the caller in the same thread as internally it uses an :ref:`async_latch`. Coroutine arguments are thread safe, so can return in any thread they choose.

.. doxygenfunction:: zab::wait_for(engine *, Promises&&...)

.. doxygenfunction:: zab::wait_for(engine *, std::vector<simple_future<T>>&&)
