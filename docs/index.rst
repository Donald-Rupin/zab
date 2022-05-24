.. _main

Welcome to ZAB's Home Page
===============================

--------------------------

`Github Repository <https://github.com/Donald-Rupin/zab>`_

--------------------------

A high-performance liburing backed event loop for building asynchronous and multi-threaded programs. 

The original goal of this library was to learn the new `coroutines TS <https://en.cppreference.com/w/cpp/coroutine>`_ for C++. I found the most difficult part of the coroutine TS is when you want to develop an asynchronous architecture or "executer/runtime" that can handle re-entrant code across possibly different threads. Thus, **ZAB** was born. 

ZAB uses io_uring and liburing as its back end to provide asynchronous system calls. ZAB so far does not try to provide QoL or improvements on the posix system call API's. However, C++ classes are provided to provide resource management and group similar function calls.

`benchmarks can be found here <https://github.com/Donald-Rupin/zab_benchmark<`

Contact: donald.rupin@pm.me

--------------------------

Disclosure 
**********
**ZAB** is still in the _alpha_ stage so all feedback and suggestions are welcome and encouraged. I do not expect the interface to change significantly but it is possible. Tests are provided and I've tried to be thorough but I don't have the capacity to test everything at this stage. If people are interested in using the **ZAB** framework, I will work to release a fully stable version. 

.. toctree::
   :maxdepth: 2
   :caption: Visit:
   :hidden:

   lib/lib_root 

..
    posts/blog_home
