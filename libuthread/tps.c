#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

/* TODO: Phase 2 */
struct tps_container {
	pthread_t tid;
	void* memAddress;
};

queue_t tps_queue = NULL;

static int find_tid(void *data, void *arg){
	thread_t a = (thread_t) data;
	int match = (int)(long) arg;

	if(a->tid == match)
		return 1;

	return 0;
}

int tps_init(int segv)
{
	/* TODO: Phase 2 */
        if(!tps_queue)
                tps_queue=queue_create();
}

int tps_create(void)
{
	/* TODO: Phase 2 */
	pthread_t tid = pthread_self();
	tps_container tps;
	int queue_check = 0;
	int fd = 3;
	off_t offset = 0;

        //check if tps exists
	queue_check = queue_iterate(tps_queue, find_tid, (void*)tid, (void**) &tps);

	//If found return
	if(queue_check != -1)
		return -1;

	//Create Tps
	tps_container current_tps = (tps_container) malloc(sizeof(tps_container));
	current_tps->tid = tid;
	current_tps->memAddress = mmap(NULL, TPS_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS, fd, offset);

	queue_enqueue(tps_queue, (void*)current_tps);

	return 0;
}

int tps_destroy(void)
{
	/* TODO: Phase 2 */
	tps_container tps;
	pthread_t tid = pthread_self();
	int queue_check;

	queue_check = queue_iterate(tps_queue, find_tid, (void*)tid, (void**) &tps);

	//If not found return
	if(queue_check == -1)
		return -1;
	
	//free tps and container
	queue_delete(tps_queue, tps);
	unmap(memAddress, TPS_SIZE);
	free(tps);
	return 0;
}


int tps_read(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
}

int tps_clone(pthread_t tid)
{
	/* TODO: Phase 2 */
}

