.. _reusable_promise:

================
reusable_promise
================

--------------------------

A ``reusable_promise<T>`` allows a coroutine to return values to the caller multiple times. ``reusable_promise<T>`` satisfies the ``Reoccurring`` concept.   

The coroutine key words available are ``co_yield (ARG)``, ``co_return (ARG)`` where T is constructible from ``ARG``, and ``co_yield (std::nullopt_t)``, ``co_return (std::nullopt_t)`` in the case the promise cannot be fulfilled.

--------------------------

.. doxygenclass:: zab::reusable_promise
   :members:
   :protected-members:
   :undoc-members: