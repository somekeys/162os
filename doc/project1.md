Design Document for Project 1: Threads
======================================
## Efficient Alarm Cloc
- Data structures and functions
- devices/timer.c:timer_sleep()   
main func, will block the thread and add the thread to a list of sleeping thread
    - struct list sleep_list  
    The list is used to store threads that are currently sleeping 
    - 
        ```
        struct sleep_list_elem 
            { 
                thread* sleeping_thread;
                int ticks;  // num of ticks the thread shoud sleep
                int start_ticks; // the ticks when thread start sleeping
                struct list_elem elem;
            }
``` 
        This list elmet is used to link the sleep_list_elem to the sleeping thread list 
    - void check_sleep();   
        This routine will iterate through the sleep_list and remove and wake up all threads that has reached the sleep time 
- Algorithms 
    timer_sleep will disable interupt and add the thread to the sleep_list. After that the function will call thread_block to put the thread to the block status. Every time the schedule() is called, it will run check_sleep() to wake up threads. There is no need to check it on every time_interupt, since the thread will not be runing until the schdule() is called.

## Priority Scheduler
- Data structures and functions
    - 
        ```
        bool less_priority(list_elem *a, list_elem *b, void *aux);
        ```

        Comparator used to get the thread with highest prority in the ready list
    -
        ```
        struct donator
            { 
            int num;
            struct list** donator_lists
            }
```
- Revise struct thread
        Add struct member `struct donator donator_groups` 

- Algorithms 
    - Choosing the next thread to run 
    The next_thread_run() will call list_max on ready_list to get the highest prority thread 
    - Choose the next lock holder
    The sema_up will unlock the waiter with higest priority using list_max
    - Donate the priority
    The lock_acquire() will incert the reference of sema->waiters into the donators_group member of the thread that gets the lock.(i.e. increase donators_groups-> num and add waiters to the end of donators_group-> donator_lists)  The respective donators reference will be set to NULL on lock_release;
    - Get priority 
    Thread_get_priority() will recursively get the priority of the thread and donators_group if exist and adds them up.
## Advanced Scheduler
- Data structures and functions
    - Modifying **struct thread**
    Add member `fixed_point_t recent_cpu , fixed_point_t nice`
    - `fixed_point_t load_avg`
    - 
- Algorithms 
    - Calculate **load_avg**
    In **thread_tick()** calculate the load_avg when (timer_ticks() % TIMER_FREQ == 0); use **list_size(ready_list) + 1** if the runing_thread is not idle_thread;
    - Update **recent_cpu**
    In **thread_tick()** increase runing thread's recent_cpu by one on every click and update acoording to load_avg on every second;
    - Update **pririty** 
    update when (timer_ticks() % 4 == 0);
    - Notes
    Since list_max select the max thread near the front and runing_thread is putted on the back,thread with same priority will automatically run in round robin fashion


