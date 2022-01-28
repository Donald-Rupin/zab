.. _proxy:

=====
proxy
=====

A ``proxy`` is a way for classes to expose asynchronous functions through their public interface that have thread based pre and post conditions. A ``proxy`` to a member function places the path of execution into the indicated thread before calling/re-entering the member function then returning the path of execution to the calling thread on exiting. In the case of ``simple_futures<T>`` and ``reusable_futures<T>`` this switch occurs on ``co_await``. If you are already in the correct thread, the proxy code no-ops. 

A proxy can only be used by classes that inherit form :ref:`engine_enabled`.

The gerneral format for a proxy call is:

.. code-block:: c++

    RET<> proxy(
        RET (your_class::*)(ARGS1...),
        thread_t,
        ARGS2...
    );

Where ``ARGS1`` is constructible from ``ARGS2``. NOTE: ``ARGS2`` is always taken by copy because using parameter packs with universal references and stacked coroutines almost always ends in undefined behavior. If your member function takes references, it will be references to the proxy created arguments, not the caller arguments. For ``simple_futures<T>`` and ``reusable_futures<T>`` this is recommended, as the proxy created arguments are guarantied to live longer then the member coroutine, and reduces an extra copy.  For ``async_function``, since there is no waiting, the copied parameters will live until the first suspension.

An example of using proxy with a ``simple_future<T>`` is:

.. code-block:: c++

    // In your class definition 

    public:

        simple_future<bool> 
        do_foo(const std::vector<std::uint8_t>& _p1, int _p2)
        {
            return proxy(
                &your_class::Foo, /* Proxies the Foo function */
                thread_t{2}      /* Foo will execute in thread 2*/
                _p1,
                _p2
                );
        }

    private:

        simple_future<bool> 
        foo(std::vector<std::uint8_t>& _f1, int _f2)
        {
            /* Here _f1 is a reference to the proxy copied _p1         */
            /* This is why we can take by reference, even though DoFoo */
            /* takes by const reference...                             */
            ...
        }

    // A user of your class via the proxy

    async_function<>
    user_of_foo()
    {
        /* We are running in thread 1 */

        /* The proxy will execute this in thread 2 */
        auto result = co_await DoFoo({1, 2, 3}, 2);

        /* We will return in thread  1*/
    }

A ``proxy`` is the only asynchronous primitive with dedicated thread coordination. It will always return into the calling thread, and always call the member function in the specified thread. It can be used for basic :ref:`async_sync` but is far more general then the constructs provided in there. A ``proxy`` will remove any benefits of parallelism for the entire member function given (which you may want). For more multi-thread friendly coordination, using the :ref:`async_sync` constructs will give better results.