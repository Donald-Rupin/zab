.. _tcp_stream:

==========
tcp_stream
==========

--------------------------

The ``tcp_stream`` represents the a duplex network stream for writing and reading data. ``read``, ``shutdown`` and ``cancel`` are not thread safe. Deconstruction or moving the ``tcp_stream`` while in use results in undefined behavior. A user should make sure to ensure to ``shutdown`` and wait for any operations to leave the functions before deconstructing the object.

.. code-block:: c++
    :caption: Example

    async_function<>
    your_class::run_stream(tcp_stream&& _stream)
    {
        /* Save locally so doesnt go out of scope when we suspend */
        tcp_stream stream(std::move(_stream));

        auto data_to_send = "hello world";
        std::size_t written = co_await stream.Write(data_to_send);

        if (written == ::strlen(data_to_send)) {

            /* Get 1 byte back as confirmation */
            auto buffer_opt = co_await stream.Read(1);

            /* Read can return less then requested but not more... */
            if (buffer_opt && buffer_opt->size()) {
                std::cout << "Received: " << buffer_opt.at(0) << "\n";
            }
        }

        /* Ensure we shut it down! (flushes all writes and wakes any readers) */
        co_await stream.shut_down();

        /* RAII will clean up for us! */
    }

--------------------------

.. doxygenclass:: zab::tcp_stream
   :members:
   :protected-members:
   :undoc-members: