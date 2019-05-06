#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"


struct semaphore {
	int count;
	queue_t blocked_queue;
};

sem_t sem_create(size_t count)
{
	sem_t sem;
	sem = (sem_t) malloc(sizeof(sem_t));

	if(sem){
		sem->count = count;
		return sem;
	}

	sem->count = count;
	sem->blocked_queue = queue_create();
	return NULL;
}

int sem_destroy(sem_t sem)
{
	if(sem == NULL || queue_length(sem->blocked_queue) > 0)
		return -1;

	free(sem);
	return 0;
}

int sem_down(sem_t sem)
{
	int tid = pthread_self();

	if(!sem)
		return -1;

	//Blocks current thread if no semaphore
	if(sem->count == 0){
		queue_enqueue(sem->blocked_queue, &tid); 
		thread_block();
	}

	sem->count--;
	return 0;	
}

int sem_up(sem_t sem)
{
	int tid;

	if(!sem)
		return -1;

	//Unblocks oldest thread if semaphore
	if(queue_length(sem->blocked_queue) > 0){
		queue_dequeue(sem->blocked_queue, (void**) &tid);
		thread_unblock(tid);
	}

	sem->count++;
	return 0;	
}

int sem_getvalue(sem_t sem, int *sval)
{
	if(!sem || !sval)
		return -1;

	if(sem->count > 0)
		*sval = sem->count;
	
	else if(sem->count == 0)
		*sval = queue_length(sem->blocked_queue) * -1;
	return 0;
}
