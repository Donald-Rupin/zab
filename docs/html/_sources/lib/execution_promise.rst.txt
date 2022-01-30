.. _execution_promise:

=================
execution_promise
=================

--------------------------

This is the most basic promise. It simply begins execution on creation, and cleans itself up after completion. ``execution_promise`` satisfies the ``Execution`` concept.

The only coroutine key word available is ``co_return (void)`` and ``co_await (AwaitableProxy)``.

--------------------------

.. doxygenclass:: zab::execution_promise
   :members:
   :protected-members:
   :undoc-members: