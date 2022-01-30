.. _pause:

=====
pause
=====

--------------------------

Pause the execution of the current coroutine until it is manually resumed. Useful for deferring for an unspecified or unknown amount of time. For example, waiting for the user to do something. 

--------------------------

.. code-block:: c++
    :caption: Example

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

--------------------------

A ``pause_pack`` has a non-owning reference to the coroutines state and also provides two options to set for resumption: The data to return and the thread to resume in.  


.. doxygenstruct:: zab::pause_pack
   :members:
   :protected-members:
   :undoc-members:

--------------------------

``pause(...)`` will give the Functor a non-owner ptr to the pause_pack. This ptr is valid until ``unpause`` has been called with it. 

//TODO: fix doxygen generation of functions with concepts...


.. doxygenfunction:: zab::pause(Functor&&)

--------------------------

After the coroutine has been paused ``thread_`` and ``data_`` can be set to the desired state and then the coroutine can be resumed using ``unpause(...)``. This resumption is thread-safe so may be called in any thread, but the coroutine will be resumed in the thread specified by  ``thread_``.



.. doxygenfunction:: zab::unpause(engine *, pause_pack&, order_t)

.. doxygenfunction:: zab::unpause(engine *, pause_pack&)