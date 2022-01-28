.. _networking:

==========
Networking
==========

ZAB networking is intended to provide low level posix network control. The goal here was not to create a higher level of abstraction for networking, but to provide a standard non-blocking asynchronous model for common network activities. At the moment, TCP/IP is only supported. More complex applications can be built on-top to provide more structured or simpler networking functionality (for example: a HTTP server). The overlay comprises of three main networking classes: `tcp_acceptor`, `tcp_connector` and `tcp_stream`.

.. toctree::
    :maxdepth: 2

    tcp_connector
    tcp_acceptor
    tcp_stream