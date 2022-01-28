.. _main

Welcome to ZAB's Home Page
===============================

A high-performance framework for building asynchronous and multi-threaded programs. 

The original goal of this library was to learn the new `coroutines TS <https://en.cppreference.com/w/cpp/coroutine>`_ for C++. I found the most difficult part of the coroutine TS is when you want to develop an asynchronous architecture or "executer/runtime" that can handle re-entrant code across possibly different threads. Thus, **ZAB** was born. 

The goal then became to provide a generic framework so users can utilise coroutines to easily develop concise, multi-threaded asynchronous code. 

The framework is fairly opinionated with the aim of providing multi-threaded and asynchronous functionality in a way that works like within high-level languages with built-in async support. The design largely takes inspiration from `libuv <https://github.com/libuv/libuv>`_ and its use with javascript in the `node.js <https://nodejs.org/en/>`_ runtime.   

Contact: donald.rupin@pm.me

Disclosure 
**********
**ZAB** is still in the _alpha_ stage so all feedback and suggestions are welcome and encouraged. I do not expect the interface to change significantly but it is possible. Tests are provided and I've tried to be thorough but I don't have the capacity to test everything at this stage. If people are interested in using the **ZAB** framework, I will work to release a fully stable version. 

.. toctree::
   :maxdepth: 2
   :caption: Contents:
 
   lib/lib_root 
