# Part 1
## Data Structure:

The data structure we used for our semaphore is just a simple sem_t struct that
contains a counter for the amount of resources and a queue of its blocked
threads.The semaphore’s job is to keep track of the amount of resources and
blocks threads whenever the counter becomes 0.

## Create:
The struct is initialized in sem_create() which provides the count and also
creates the queue. It returns a semaphore if successful or NULL if unsuccessful.
NULL is only returned if malloc fails to allocate memory to the semaphore
struct.

## Destroy:
The function simply frees the semaphore and returns 0 on success. If the sem is
NULL or the queue is not empty then it returns -1s.

## Sem Down:
This is one of the actual uses of a semaphore takes place. Sem down will try to
take a resource from the semaphore. We have a while loop that checks if the
count is 0 which will enqueue the thread in the queue and then block it. This
guarantees that the thread will stay blocked when a resources is freed and
another thread takes it while the current thread is waking up. If the resource
is free then the thread takes it. This part is all in a critical section to
ensure that the thread is properly enqueued.

## Sem Up:
Sem up is similar to sem down but will instead free a resource. If there is
something in the blocked queue, then it will dequeue it and unblocks that
thread. This is also in a critical section for atomic queue operation.

## Get Value:
Get value simply provides the semaphores resources to the sval argument. If
there are no resources then it will return the negative of the queue length
since this symbolizes the amount of blocked threads. 

## Testing:
We simply used the testers provided to check if our semaphore implementation
works. All the outputs of each test are all within expected results. Further,
testing involves using the TPS implementation to test whether synchronization
happens with multiple threads.

# Part 2
Data Structure:
The data structure we use to associate a thread to its tps is a struct
(tps_container_t) that contains both the thread’s tid and a pointer to the data
structure that contains the memory address. The data structure that holds the
memory address is a struct (pages_t) that contains the the memory address and
the reference counter. Upon initiation of the tps api, a global queue is created
to store all the tps_container_ts that are created. We use a queue because it
can act as a linked list and the queue_iterate() function allows us to easily
locate a specific tid. 

## Create:
When we call tps_create(), we have to first iterate through the queue to make
sure that the current thread does not already have a tps created. If there is
already a tps created for the thread, then a tps is not created and the function
returns -1 to signify an error has occurred. Otherwise, a tps is created using
mmap and the memory address is saved in a pages_t struct; the pages_t struct
along with the thread’s tid is saved in a tps_container_t struct. The memory
address allocated will be set to PROT_NONE, which means that the contents cannot
be accessed at all. The protection changes depending on the function being
called on the tps. Then, the tps_container_t is enqueued. 

## Clone:
In order to clone a tps, we have to first check that the tps being cloned even
exists and that the thread that is getting the clone doesn’t already have a tps.
To do this check, we iterate through the queue; the thread with the tps that is
being cloned should be found while the thread that is taking the clone should
not be found. Then, similar to the creation of a tps, a tps_container_t and
pages_t is allocated and associated to the thread. However, the memory address
that it contains will be the same as the target tps and the reference counter is
incremented. Both threads will point to the same tps until either one of them
call tps_write() and alter the contents in their tps.

## Read/Write:
When reading and writing, similar error checks as the other functions are done.
When writing to a tps, the reference counter is checked to see if it is greater
than or equal to 1. If it is greater than 1, then it means that multiple threads
are sharing the same tps. In this case, first, a new memory address is
allocated, then the contents of the original memory page is copied to the new
one by setting the original memory page protection to read and then new memory
page’s protection to write. Then the new memory page becomes the thread’s tps.
After that, the tps is written to normally and both thread’s protection is reset
to no access. We change the protection depending on the function because we do
not want functions to have access that it should not have. When reading, we set
the protection to read and write the contents to a buffer. 

## Testing:
To test the basic implementation we used the provided tester, tps.c, as our base
tester. Once that works as expected, we made our own testers to further test our
code. Tps_tester.c is a variation of tps.c but exhausts our TPS implementation
by making sure we go through each if statement and checking for expected
outcomes. This tester also checks for offsetting data which was not  tested in
the tps.c. 

To test the segv handler, we made a very simple tester that is identical to the
one provided in discussion. We believe that this is enough to test that going
through protected storages will trigger the seg handler and find it in one of
our threads. We also did a simple test by commenting out one of the mprotects()
in the tps read/write functions that protects the storage and see if that
triggers the handler. Through this we found that it works successfully. 

