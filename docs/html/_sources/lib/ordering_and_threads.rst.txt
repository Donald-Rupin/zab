.. _ordering_and_threads:

====================
Ordering and Threads
====================

order_t
-------

An ``order_t`` is a strongly typed ``std::size_t``. This specifies the delay to apply in the event loop in nanoseconds (0 for no delay).

.. doxygenstruct:: zab::order_t
   :members:
   :protected-members:
   :undoc-members:

thread_t
--------

A ``thread_t`` is a strongly typed ``std::uint16_t`` which is used to select a thread to be used. This can have different meaning in different contexts.  Typically ``thread_t::any_thread()`` can be used to select which thread currently has the least amount of work to do. Thread bounds are not checked, so specifying an index larger than the number of threads in the ``engine`` is undefined behavior. 

.. doxygenstruct:: zab::thread_t
   :members:
   :protected-members:
   :undoc-members: