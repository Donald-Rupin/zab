.. _async_barrier:

==============
async_barrier
==============

--------------------------

A ``async`barrier`` is the equivalent of the `std::barrier <https://en.cppreference.com/w/cpp/thread/barrier>`_.


    The class template std::barrier provides a thread-coordination mechanism that allows at most an expected number of threads to `suspend` until the expected number of threads arrive at the barrier. Unlike std::latch, barriers are reusable: once the arriving threads are `resumed` from a barrier phase's synchronization point, the same barrier can be reused. 

    A barrier object's lifetime consists of a sequence of barrier phases. Each phase defines a phase synchronization point. Threads that arrive at the barrier during the phase can `suspend` on the phase synchronization point by calling wait, and will be `resumed` when the phase completion step is run. 


See `Unbounded use of async_barrier`_ for some oddities around phases and phase sequencing. This can occur more evidently then the thread based ``std::barrier``.

    A barrier phase consists following steps: 
       + The expected count is decremented by each call to arrive[ , arrive`and`wait] or arrive`and`drop.
       + When the expected count reaches zero, the phase completion step is run. The completion step invokes the completion function object, and `resumes all suspended frames` on the phase synchronization point. The end of the completion step strongly happens-before the returns from all calls that were `suspended` by the completion step.
       + When the completion step finishes, the expected count is reset to the value specified `(barrier count)` at construction less the number of calls to arrive`and`drop since, and the next barrier phase begins. 
    Concurrent invocations of the member functions of barrier, except for the destructor, do not introduce data races.

The implementation differs (so far) in terms of ``arrival`token`` and the use of ``arrival`token arrive()``. 
+ You cannot specifiy an amount to decrement for ``arrive()``
+ The lifetime of the ``arrival`token`` must be greater then the phase it arrived out (see `Unbounded use of async_barrier`_)
+ The ``arrival`token`` can live past its own phase and the next phase without causing undefined behavior
+ There is no corresponding ``wait(arrival`token&&)`` function and suspension can be achieved by ``co`await``ing the ``arrival`token``. 

With ``async`barrier`` the completion function is allowed to be asynchronous. If the completion function satisfies ``NoThrowAwaitable`` the barrier will ``co`await`` the function before continuing. 

--------------------------

.. code-block:: c++
    :caption: Example

    simple_future<> 
    your_class::do_map();

    using ExampleBarrier = async_barrier<std::function<async_function<>()>>;

    async_function<> 
    your_class::do_reduce(
        thread_t _thread, 
        std::shared_ptr<ExampleBarrier> _barier, 
        std::size_t _iterations)
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



Unbounded use of async_barrier
------------------------------

`std::experimental::barrier <https://en.cppreference.com/w/cpp/experimental/barrier>`_ (although ``std::barrier`` does not) mentions the concept of "thread binding" which requires that the same threads be used every time, and therefore the number of threads using the ``std::barrier`` must remain constant (ignoring ``arrive_and_drop``) and must be equal to the ``barrier count``.

``async_barrier`` can not only handle a arbitrary amounts of different threads, the same thread can arrive at the barrier multiple times by suspending corountines. As a result of this, for a barrier phase there might actually be more then the ``barrier count`` frames suspended. This can lead to a phonenum for queued phase waiting. This happens more evidently as ``async_barrier`` will never block. So we must suspend the co-routines instead of blocking until they can successfully become part of a phase (decrement the counter).  

In terms of function guaranties, the "current phase" is not the currently executing phase, but the phase that its arrival frame will form part of. This is important when considering when a function strongly happens-before the start of the phase completion step for the current phase.  

For example, a barrier that has a ``barrier count`` of 5, there might be 15 frames currently suspended at the barrier. This would be very rare and would require some odd thread scheduling by the OS (priority inversion limiting the control thread) or user code blocking the control thread. Essentially, the exhibited behavior would be as expected: as if the 10 extra frames have not suspended yet but are about to. The control thread calls the completion function then will resume the first 5 frames. If the amount of left over frames is greater then ``barrier count`` a new control thread is picked to repeat. The application would see: ``[completion_function() -> process 5 -> completion_function() - > process 5 -> completion_function() -> process 5]`` in rapid succession.

The use of ``arrive_and_drop()`` seemingly adheres to the standard, but in the presence queued phase waiting might behave oddly according to your application. According to the published ``std::barrier``, ``arrive_and_drop()`` does not block. So this function will never block even if the drop in count does not count towards the next executing phase. In the above example, ``arrive_and_drop()`` could be called before the first phase is complete, but it applies to the last phase queued. This means that ``arrive_and_drop()`` returns, but the phase it applies to has not yet started. In this case the application would see: ``[completion_function() -> process 5 -> completion_function() - > process 5 -> completion_function() -> process 5 -> decrement barrier count]`` in rapid succession but ``arrive_and_drop()`` may return before this occurs.

--------------------------

.. doxygenclass:: zab::async_barrier
   :members:
   :protected-members:
   :undoc-members: