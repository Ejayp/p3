#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

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

	if(sem == NULL)
		return NULL;

	sem->count = count;
	sem->blocked_queue = queue_create();
	return sem;
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
	pthread_t tid = pthread_self();

	if(!sem)
		return -1;

	enter_critical_section();
	//Blocks current thread if no semaphore
	while(sem->count == 0){
		queue_enqueue(sem->blocked_queue, (void*)tid); 
		thread_block();
	}

	sem->count--;

	exit_critical_section();
	return 0;
}

int sem_up(sem_t sem)
{
	pthread_t tid;

	if(!sem)
		return -1;

	enter_critical_section();
	//Unblocks oldest thread if semaphore
	sem->count++;
	if(queue_length(sem->blocked_queue) > 0){
		queue_dequeue(sem->blocked_queue, (void**) &tid);
		thread_unblock(tid);
	}

	exit_critical_section();
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
