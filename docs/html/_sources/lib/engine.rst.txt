.. _engine:

======
engine
======

--------------------------

Include ``"zab/engine.hpp"``

--------------------------

The ``engine`` is the core object in ZAB which represents the framework. The ``engine`` is responsible for running the event loops, ordering events for execution, handling cross thread events, and providing both :ref:`signal_handling` and asynchronous io through the :ref:`event_loop`. 

The ``engine`` is constructed with `engine::configs` to configure it at runtime.

--------------------------


.. doxygenstruct:: zab::engine::configs
   :members:
   :protected-members:
   :undoc-members:

--------------------------

.. doxygenclass:: zab::engine
   :members:
   :protected-members:
   :undoc-members: