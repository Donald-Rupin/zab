.. _async_binary_semaphore:

======================
async_binary_semaphore
======================

A ``async_binary_semaphore`` is the equivalent of the `std::binary_semaphore <https://en.cppreference.com/w/cpp/thread/counting_semaphore>`_. 

    binary_semaphore is an alias for specialization of std::counting_semaphore with LeastMaxValue being 1. Implementations may implement binary_semaphore more efficiently than the default implementation of std::counting_semaphore.

Same description and basic interface as ``async_counting_semaphore``. 

Note:
    Semaphores are also often used for the semantics of signalling/notifying rather than mutual exclusion, by initializing the semaphore with 0 and thus blocking the receiver(s) that try to acquire(), until the notifier "signals" by invoking release(n).

For mutual exclusion see :ref:`async_mutex`.

.. code-block:: c++
    :caption: Example
    
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

        async_counting_semaphore<1> sem(engine_, false);

        /* Create worker [logical] threads 1 - n */
        const auto threads = engine_->number_of_workers();
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


.. doxygentypedef:: zab::async_binary_semaphore

.. doxygenclass:: zab::async_counting_semaphore< 1 >
   :members:
   :protected-members:
   :undoc-members: