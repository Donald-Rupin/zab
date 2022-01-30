.. _observable:

===========
Observables
===========

--------------------------

An ``observable<Args...>`` represents a thread safe publish/subscribe system. The to main operations are ``emit`` and ``connect``.

``emit`` is a ``simple_future<>`` that safely resumes all waiting subscribers allowing them access to the given parameters. For safety the parameters of emit and forwarded into a safe location. As to ensure no copies are made, move objects in as arguments. ``emit`` will resume the caller only once `all` observers have received the publish, and have finished with the data. There is a ``async_emit`` for pushing ab ``emit`` in the background. Deconstruction of the ``observable<Args...>`` when ``async_emit`` are running is undefined behavior. 

``connect`` is a ``simple_future<>`` that safely subscribes to an ``observable<Args...>`` and returns an ``observer`` that can be used to await for publishes. ``co_await observer`` on resumption will return an ``observer_guard`` that ensures the lifetime of the published values. The published values can be accessed through ``const std::tuple<Args...>& observer_guard::event()``.

--------------------------

.. code-block:: c++
    :caption: Example

    using string_observable = observable<std::string, std::string>;

    async_function<> 
    your_class::subscriber(string_observable& _ob)
    {
        auto con = co_wait _ob.connect();

        while (true) {

            auto guard = co_await con;

            const auto&[s1, s2] = guard.event();

            /* On guard deconstruct the publisher is notified that all    */
            /* observers have finished and the strings are deconstructed  */
        }

    }

    async_function<> 
    your_class::publisher()
    {
        string_observable ob(engine_);

        subscriber(ob);

        while (true) {
            co_await ob.emit("hello", "world"); 
        }
    }

--------------------------

.. doxygenclass:: zab::observable
   :members:
   :protected-members:
   :undoc-members: