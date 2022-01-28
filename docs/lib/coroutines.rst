.. _coroutines:

==========
Coroutines
==========

Coroutine types represent different asynchronous functions types available as apart of the ZAB library. The coroutine :ref:`task_types` affects how the function can be called. The body of the coroutine code can be configured by :ref:`promise_types`. All coroutine types take a corresponding promise type as a template parameter.

ZAB currently supports: ``async_function<Execution>``, ``simple_future<T, Returnable>`` and ``reusable_future<T, Reoccurring>``. 

.. toctree::
    :maxdepth: 2
    
    task_types
    promise_types