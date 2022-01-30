.. _async_sync:

============================
Asynchronous Synchronisation
============================

--------------------------


ZAB provides synconisation mechanisms similar to those provided by the `C++ standard thread support library <https://en.cppreference.com/w/cpp/thread>`_. The main difference is that these synchronisation mechanisms are `non-blocking` in terms of the event loop. This allows mutual exclusion and thread co-ordination without interfering with the event loops. This also allows the same thread to "wait" or "co-ordinate" with itself. For example, the same thread may have different coroutines all blocking on the same ``async_mutex``.  

.. toctree::
    :maxdepth: 2

    pause_token
    async_counting_semaphore
    async_binary_semaphore
    async_mutex
    async_latch
    async_barrier