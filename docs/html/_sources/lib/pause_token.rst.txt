.. _pause_token:

===========
pause_token
===========

--------------------------

A ``pause_token`` is the only synchronisation class in ZAB that has no standard library equivalent. It is used for pauseing and unpausing coroutines in a thread safe manner. All ``pause_token`` functions except the deconstructor are thread safe. The ``pause_token`` is constructed in a paused state.

If paused, coroutines that ``co_await`` the ``pause_token`` will have their execution suspended. Otherwise, ``co_await`` no-ops.

If the ``pause_token`` changes from a paused state to an unpaused state, all coroutines that had their execution suspended will be resumed in the `same` thread they were suspended in. 

The ``pause_token`` is re-usable and can be switched between paused and unpaused states. There is no maximum number of coroutines that can be suspended. Although, unpausing is linear in the number of suspended coroutines. 

--------------------------

.. code-block:: c++
    :caption: Example

    async_function<> 
    your_class::worker(thread_t _thread, pause_token& _pt)
    {   
        /* Move into our thread */
        co_await yield(_thread);

        /* If it was already unpaused this will just no-op */
        co_await _pt;

        std::cout << "At work " << _thread << "\n";
    }


    async_function<> 
    your_class::pause_example(pause_token& _pt)
    {
        const auto threads = engine_->number_of_workers();
        pause_token pt(engine_);

        for (std::uint16_t t = 0; t < threads; ++t) {
            worker(thread_t{t}, pt);
        }

        /* wait 5 second... */
        co_await yield(order::seconds(5));

        std::cout << "Putting to work\n";
        pt.unpause();
    }

--------------------------

.. doxygenclass:: zab::pause_token
   :members:
   :protected-members:
   :undoc-members: