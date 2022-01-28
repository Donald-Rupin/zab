.. _signal_handling:

===============
Signal Handling
===============

The ``engine`` provides rudimentary signal handling that utilises the "self-pipe trick". Your class can access the ``signal_handler`` through ``signal_handler& engine::get_signal_handler()`` and use this to register for notification every time a signal is caught.

.. doxygenclass:: zab::signal_handler
   :members:
   :protected-members:
   :undoc-members: