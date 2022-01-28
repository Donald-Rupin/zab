.. _event_loop:

==========
event_loop
==========

AB is powered by `liburing <https://github.com/axboe/liburing>`_ which is a userspace wrapper for `io_uring <https://kernel.dk/io_uring.pdf>`_ An ``event_loop`` loop exists for every ``engine`` thread in ZAB. This thread loops on the completion queue waiting for completed io requests then fowarding the results to and resuming the respective coroutines. User space events (those submitted by ``engine::resume`` and family) are submitted to the ``event_loop`` via an event_fd. User code should only do short computations or ``yield`` long running computation in the ``event_loop`` to ensure IO completions are reported back in a timely manner.  

In this release the following io_uring calls are supported in ZAB:

* `open_at <https://linux.die.net/man/2/openat>`_
* `close <https://linux.die.net/man/2/close>`_
* `read <https://linux.die.net/man/2/read>`_
* `read_v <https://linux.die.net/man/2/readv>`_
* `write <https://linux.die.net/man/2/write>`_
* `write_v <https://linux.die.net/man/2/writev>`_
* `recv <https://linux.die.net/man/2/recv>`_
* `send <https://linux.die.net/man/2/send>`_
* `accept <https://linux.die.net/man/2/accept>`_
* `connect <https://linux.die.net/man/2/connect>`_

As in ``liburing`` options can be attempted to be canclled. Each io_uring call has two overloads: 
1. A simple future that has a ``io_handle`` out value which will be set to the cancellation handle on suspension. 
2. A void function that expects a ``io_handle`` prefilled with the suspended coroutines handle. In either case, the value can be passed to ``cancel_event`` to attempt a cancle. 

A ``io_handle`` is an alias to a ``pause_pack*`` which can be obtained through the ``zab::pause(...)`` function.

.. doxygenclass:: zab::event_loop
   :members:
   :protected-members:
   :undoc-members: