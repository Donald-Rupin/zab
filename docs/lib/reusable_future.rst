.. _reusable_future:

===============
reusable_future
===============

A function can mark itself as a coroutine by having a return type of ``reusable_future<T, Promise>``. A reusable future is essentially ``simple_future<T, Promise>`` that can be ``co_await``'ed multiple times. The return type is always wrapped in an ``std::optional`` since the ``reusable_future<T, Promise>`` may have finished or hit an error. A user can use the ``bool complete()`` to distinguish the two. A ``reusable_future<T, Promise>`` can be viewed as a generator that may exhibit asynchronous behavior. A ``reusable_future<T>`` will wake up the waiting coroutine and return a value to when the ``Promise`` uses either the ``co_yield`` or ``co_return`` keywords. ``co_yield`` will return a value, but allow a the ``reusable_future<T, Promise>`` to be ``co_await``ed again. ``co_return`` will complete the coroutine and any further ``co_await`` attempts on the ``reusable_future<T, Promise>`` will fail.

The difference in use cases between using a ``simple_future<T. Promise>`` many times and having a single ``reusable_future<T, Promise>`` is that a ``reusable_future<T, Promise>`` can store state about its context between value returns. 

The promise type for an ``reusable_future<T, Promise>`` must satisfy the ``Reoccurring`` concept. The default promise type is the :ref:`reusable_promise`.

.. code-block:: c++
    :caption: Example
    
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

.. doxygenclass:: zab::reusable_future
   :members:
   :protected-members:
   :undoc-members: