.. _async_mutex:

===========
async_mutex
===========

--------------------------


The ``async_mutex`` is a merger of a `std::mutex <https://en.cppreference.com/w/cpp/thread/mutex>`_ and a `std::lock_guard <https://en.cppreference.com/w/cpp/thread/lock_guard>`_. 

    The mutex class is a synchronization primitive that can be used to protect shared data from being simultaneously accessed by multiple _coroutines_.

    mutex offers exclusive, non-recursive ownership semantics: 

With ``async_mutex``, ``non-recursive`` and locking multiple times in the same thread are different. Multiple independent coroutines in the same thread can attempt to acquire the ``async_mutex``. Although, if a coroutine acquires the lock, that same coroutine or any coroutines it is currently ``co_await``ing (to any depth) cannot attempt to acquire the ``async_mutex``. This will lead to a deadlock.  

    A calling _coroutine_ owns a mutex from the time that it successfully calls either _operator co_await_ or try_lock until it calls unlock.

    When a _coroutine_ owns a mutex, all other _coroutines_ will block (for calls to _operator co_await_) or receive a false return value (for try_lock) if they attempt to claim ownership of the mutex.

    A calling _coroutine or dependents on the coroutine_ must not own the mutex prior to calling _operator co_await_ or try_lock. 

    The behavior of a program is undefined if a mutex is destroyed while still owned by any threads, or a thread terminates while owning a mutex. 

    mutex is neither copyable nor movable. 

With regards to the return value of ``operator co_await``, ``async_lock_guard``:

    The class lock_guard is a mutex wrapper that provides a convenient RAII-style mechanism for owning a mutex for the duration of a scoped block. 

    When a lock_guard object is created, it attempts to take ownership of the mutex it is given. When control leaves the scope in which the lock_guard object was created, the lock_guard is destructed and the mutex is released.

    The lock_guard class is non-copyable. 

--------------------------


.. code-block:: c++
    :caption: Example

    struct ProectedObject {
        async_mutex mtx_;
        int needs_protecting_;
    }

    async_function<> 
    set_value(ProectedObject _object, int _value)
    {   
        /* suspend until we can get the lock */
        async_lock_guard lock = co_await _object.mtx_;

        /* We can safely edit the value      */
        _object.needs_protecting_ = _value;

        /* We can safely exhibit asynchronous behavior without deadlocks */
        co_await yield();

        /* lock will be released by RAII                           */
        /* Note: resumption of blocked coroutines is not done here */
        /* Resumption is yielded and performed later...            */
    }

--------------------------


.. doxygenclass:: zab::async_mutex
   :members:
   :protected-members:
   :undoc-members:
