.. _tcp_acceptor:

============
tcp_acceptor
========

--------------------------

The ``tcp_acceptor`` represents the traditional server side of a connection. It will listen for connections on an inbound port. Deconstruction or moving the ``tcp_acceptor`` while in use results in undefined behavior. 

--------------------------

.. code-block:: c++
    :caption: Example

    async_function<>
    your_class::run_acceptor()
    {
        tcp_acceptor acceptor(engine_);
        // listen for IPv4 on port 8080 with a maximum backlog of 10
        // See bind(2) and listen(2) for more details...
        if (acceptor.listen(AF_INET, 8080, 10))
        {
            while (true)
            {
                std::optional<tcp_stream> stream_opt = co_await acceptor.accept();

                if (stream_opt) 
                { 
                    std::cout << "Got a connection\n"; 
                }
                else
                {
                    std::cout << "Got a error" << acceptor.last_error() << "\n";
                }
            }
        }
    }

--------------------------

.. doxygenclass:: zab::tcp_acceptor
   :members:
   :protected-members:
   :undoc-members:

