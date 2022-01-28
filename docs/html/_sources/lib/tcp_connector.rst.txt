.. _tcp_connector:

=============
tcp_connector
=============

The ``tcp_connector`` represents the traditional client side of a connection. It will attempt to connect to a listening host. Deconstruction or moving the ``tcp_connector`` while in use results in undefined behavior. 

.. code-block:: c++
    :caption: Example

    your_class::run_connector()
    {
        tcp_connector connector(engine_);

        /* Fill out posix struct for host details... */
        struct addrinfo  hints;
        struct addrinfo* addr;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        auto success      = getaddrinfo("localhost", "8080", &hints, &addr);

        if (success)
        {
            for (auto current = addr; (bool) addr; addr = addr->ai_next)
            {

                std::optional<tcp_stream> stream_opt = co_await connector_.connect(
                    (struct sockaddr_storage*) current->ai_addr,
                    sizeof(*current->ai_addr));

                if (stream_opt) { 
                    std::cout << "Got a connection\n"; 
                    break;
                }
                else
                {
                    std::cout << "Got a error" << connector_.last_error() << "\n";
                }
            }

            freeaddrinfo(addr);
        }
    }

.. doxygenclass:: zab::tcp_connector
   :members:
   :protected-members:
   :undoc-members:

