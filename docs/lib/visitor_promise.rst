.. _visitor_promise:

==============
vistor_promise
==============

A ``visitor_promise<T>`` is an extension of :ref:`simple_promise` that allows deferred execution of some function/s to "visit" or "preview" the returned type. ``visitor_promise<T>`` satisfies the ``Returnable`` concept.  

The ``visitor_promise<T>`` allows the user to save a/many callable/s of signature ``void(T&)`` which will executed during the final suspend phase of the coroutine (after ``co_return`` is called). If multiple callable's are saved, they will be executed in reverse order of deferment.

A code block can be deferred by using the ``co_yield (VistorBlock<T>)`` operator. All deferred code blocks can be removed through the ``co_yeild (purge_block)`` operator. These operators do not actually suspend the coroutine.

.. code-block:: c++
    :caption: Example

    /*
    * template<typename T = void>
    * using visitor_future = simple_future<T, visitor_promise<T>>;
    * 
    * This function would return 3.
    */
    visitor_future<std::size_t> 
    always_odd() noexcept 
    {
        co_yield defer_block(
            [this, code_]
            (std::size_t& _size) noexcept
                {
                    std::cout << "Tried to return " << _size << "\n";
                    /* Make sure the return value is odd... */
                    _size |= 1;
                }
            );


        co_return 2;
        /* The defer blocks will run after this return... */
    }

.. doxygenclass:: zab::visitor_promise
   :members:
   :protected-members:
   :undoc-members: