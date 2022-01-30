.. _async_counting_semaphore:

========================
async_counting_semaphore
========================

--------------------------


A ``async_counting_semaphore`` is the equivalent of the `std::counting_semaphore <https://en.cppreference.com/w/cpp/thread/counting_semaphore>`_.

    A counting_semaphore is a lightweight synchronization primitive that can control access to a shared resource. Unlike a std::mutex, a counting_semaphore allows more than one concurrent access to the same resource, for at least LeastMaxValue concurrent accessors. The program is ill-formed if LeastMaxValue is negative.

    A counting_semaphore contains an internal counter initialized by the constructor. This counter is decremented by calls to _operator co_await_, and is incremented by calls to release(). When the counter is zero, operator co_await suspends until the counter is incremented, but try_acquire() does not block; 

``try_acquire_for()`` and ``try_acquire_until()`` are not included in this interface.

--------------------------


.. code-block:: c++
   :caption: Example

   guaranteed_future<size_t> 
   your_class::to_wake_up();

   async_function<> 
   your_class::do_work(thread_t _thread, async_counting_semaphore<>& _sem)
   {
      /* Move into our thread */
      co_await yield(_thread);

      /* Consumer loop wont block as it suspends... */
      while (true)
      {

         /* Process work as it comes in... */
         co_await _sem;

         /* Do some work... */
      }
   }

   async_function<> 
   your_class::queue_work()
   {
      /* Move into thread 0 */
      co_await yield(thread_t{0})

      async_counting_semaphore<> sem(engine_, 0);

      /* Create worker [logical] threads 1 - n */
      const auto threads = engine_->number_of_workers();
      assert(t > 1);
      for (std::uint16_t t = 1; t < threads; ++t)
      {
         do_work(thread_t{t}, sem);
      }

      /* Producer loop wont block as it suspends... */
      while (true)
      {
         /* Get work from somewhere */
         auto to_wake_up = co_await to_wake_up();

         /* notify to wake up threads */
         sem.release(to_wake_up);
      }
   }

--------------------------


.. doxygenclass:: zab::async_counting_semaphore
   :members:
   :protected-members:
   :undoc-members:
