.. _yield:

=====
yield
=====

--------------------------

Defer computation of the current function by passing control back to the event loop. This defers the execution of the function without blocking the event loop. User are able to specifiy a delay to the yield or specify a thread to return into.

--------------------------

.. code-block:: c++
    :caption: Example

    async_function<>
    your_class::delay() noexcept 
    { 
        /* wait 10 seconds then resume in thread 2. */
        co_await yield(
            order::now() + order::seconds(10),
            thread_t{2}
            );
    }

Classes that inherit from :ref:`engine_enabled` can forgo the ``engine*`` paremeter. 

--------------------------

.. doxygenfunction:: zab::yield(engine *)

.. doxygenfunction:: zab::yield(engine *, thread_t)

.. doxygenfunction:: zab::yield(engine *, order_t, thread_t)