.. _engine_enabled:

==============
engine_enabled
==============

--------------------------


``engine_enabled<Base>`` is a helper class that uses `CRTP <https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern>`_ to aid in setting up you classes for easy use of the ``engine`` class. It also provides configurations and member function bindings to remove common boiler plate.

If the base class satisisfies the ``details::HasInitialise<F>`` concept (has a ``void initialise() noexcept``) method, this method will be called on engine start.

If the base class satisisfies the ``details::HasMain<F>`` concept (has a ``void main() noexcept``) method, this method will be called on a loop of cadence kMainCadence.

The base class can configure its bindings by overiding the members ``zab::details::configs`` which is a parent of ``engine_enabled<Base>``.

--------------------------

.. doxygenstruct:: zab::details::configs
   :members:
   :protected-members:
   :undoc-members:




zab::engine_enabled
-------------------
The ``engine_enabled<Base>`` provides easy access to :ref:`async_primitives` and use of the ``proxy`` primitive. 

--------------------------

.. doxygenclass:: zab::engine_enabled
   :members:
   :protected-members:
   :undoc-members: