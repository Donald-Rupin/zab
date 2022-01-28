.. _simple_promise:

==============
simple_promise
==============

A ``simple_promise<T>`` is the simplest "returning" promise. The internal state maintains an ``optional<T>`` which is set by the coroutine body. This allows a [coroutine Type](#Coroutine-Types) to pull out the return type and provide it to the caller. ``simple_promise<T>`` satisfies the ``Returnable`` concept.

The coroutine key words available are ``co_return (ARG)`` where T is constructible from ``ARG``, ``co_return (std::nullopt_t)`` in the case the promise cannot be fulfilled and ``co_await (AwaitableProxy)`` for suspension.


.. doxygenclass:: zab::simple_promise
   :members:
   :protected-members:
   :undoc-members: