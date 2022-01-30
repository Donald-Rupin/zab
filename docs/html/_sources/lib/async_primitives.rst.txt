.. _async_primitives:

=======================
Asynchronous Primitives
=======================

--------------------------

Classes that inherit from ``engine_enabled<your_class>`` streamlined access to asynchronous primitives. All but :ref:`proxy` can be used outside of a ``engine_enabled<your_class>`` but require an additional ``engine*`` parameter and have no default parameters.  

.. toctree::
    :maxdepth: 2

    yield
    pause
    wait_for
    first_of
    proxy