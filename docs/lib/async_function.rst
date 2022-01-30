.. _async_function:

==============
async_function
==============

--------------------------


A function can mark itself as a coroutine by having a return type of ``async_function``. 

The promise type for an ``async_function`` must satisfy the ``Execution`` concept. The default promise type is the :ref:`execution_promise`.

--------------------------


.. code-block:: c++
    :caption: Example
    
    async_function<> 
    foo() noexcept 
    {
        /* async behaviour goes here... */
    }

--------------------------

.. doxygenclass:: zab::async_function
   :members:
   :protected-members:
   :undoc-members: