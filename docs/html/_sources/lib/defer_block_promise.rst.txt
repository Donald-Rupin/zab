.. _defer_block_promise:

===================
defer_block_promise
===================

--------------------------


A ``defer_block_promise`` is an extension of ``execution_promise`` that allows deferred execution of some function/s.  ``defer_block_promise`` satisfies the ``Execution`` concept.

The coroutine key words available are ``co_return (void)``, ``co_yield (defer_block)``, ``co_yeild (purge_block)``. The coroutine can also suspend using ``co_await (AwaitableProxy)``.

The ``defer_block_promise`` allows the user to save a/many callable/s of signature ``void(void)`` which will executed during the final suspend phase of the coroutine (after ``co_return`` is called). If multiple callable's are saved, they will be executed in reverse order of deferment.

A code block can be deferred by using the ``co_yield (defer_block)`` operator. All deferred code blocks can be removed through the ``co_yeild (purge_block)`` operator. These operators do not actually suspend the coroutine.

Access to local variables in the functions scope is more or less undefined behavior. For example, capturing from the function scope by reference. The deconstuctors will have been called but the memory would have not been de-allocated. Accessing local variables with trivial / no de-constructors (ints) is not a memory violation. Experimentation indicates correct behavior although optimizations present or future could easier mess it up. Accessing local variables with non-trivial de-constructors is undefined behavior. 

--------------------------


.. code-block:: c++
    :caption: Example

    async_function<defer_block_promise> 
    defer_example() noexcept 
    {
        co_yield defer_block(
            []
            () noexcept
                {
                    std::cout << "This will not run :(\n";
                }
            );

        /* Remove all defer blocks... */
        co_yield purge_block{};

        co_yield defer_block(
            []
            () noexcept
                {
                    std::cout << "This will run 2nd :)\n";
                }
            );

        co_yield defer_block(
            []
            () noexcept
                {
                    std::cout << "This will run  1st :)\n";
                }
            );

        co_return;
        /* The defer blocks will run after this return... */
    }

--------------------------


.. doxygenclass:: zab::defer_block_promise
   :members:
   :protected-members:
   :undoc-members: