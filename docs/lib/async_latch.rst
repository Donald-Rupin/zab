.. _async_latch:

===========
async_latch
===========

--------------------------


An `async_latch` is the equivalent of the `std::latch <https://en.cppreference.com/w/cpp/thread/latch>`_.

    The latch class is a downward counter of type std::ptrdiff_t which can be used to synchronize threads. The value of the counter is initialized on creation. _Coroutines will be suspended_ on the latch until the counter is decremented to zero. There is no possibility to increase or reset the counter, which makes the latch a single-use barrier. 

    Concurrent invocations of the member functions of latch, except for the destructor, do not introduce data races. 

:ref:`wait_for` is an good example for where latches are useful. In particular the ability to use within the same thread multiple times.

--------------------------


.. code-block:: c++
    :caption: Example

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
        const auto threads = engine_->number_of_workers();
        async_latch latch(engine_, threads + 1);

        /* Create worker [logical] threads 0 - n */
        for (std::uint16_t t = 0; t < threads; ++t) {
            do_work(thread_t{t}, latch);
        }

        co_await _latch.arrive_and_wait();

        /* If we get here, all coroutines are in the correct thread  */
        /* and completed their pre-work */
    }

--------------------------


.. doxygenclass:: zab::async_latch
   :members:
   :protected-members:
   :undoc-members:
