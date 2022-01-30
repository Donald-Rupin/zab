.. _file_io:

=======
File IO
=======

The ``async_file`` provides simple asynchronous read/write operations to the user. An ``async_file`` is essentially just a wrapper for file io family of requests providied by liburing. 

``async_file`` usage in an ``engine_enabled`` class:

--------------------------

.. code-block:: c++
    :caption: Example
    
    async_function<> 
    your_class::file_io_example()
    {
        
        async_file<char> file(engine_);
        
        /* Open with read, write, and truncate */
        bool success = co_await file.open("test_file.txt", file::Options::kRWTruncate);

        if (success)
        {
            std::vector<char> buffer(42, 42);

            /* Write data to file */
            success = co_await file.write_to_file(buffer);

            if (success)
            {
                /* Re position the file ptr */
                success = file.position(0);

                if (success)
                {
                    /* Read the data */
                    std::optional<std::vector<char>> data = co_await file.read_file();

                    if (data && *data == buffer) { std::cout << "File io was successful.": }
                }
            }

            /* Close the file */
            success = co_await file.close();
        }
    }

--------------------------

.. doxygenclass:: zab::async_file
   :members:
   :protected-members:
   :undoc-members: