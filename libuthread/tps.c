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
	int test;
};

queue_t tps_queue;

//data: tps_container_t
//arg: tid to match with
int find_tid(void *data, void *arg){
	tps_container_t a = (tps_container_t) data;
	pthread_t checkTID = (pthread_t) a->tid;
	pthread_t* match = (pthread_t*) arg;

	printf("pthreadfind: %d\n", (int)checkTID);
	printf("pthreadmatch %d\n", (int)*match);

	if(checkTID == *match)
		return 1;
	
	return 0;
}

int tps_init(int segv)
{
	/* TODO: Phase 2 */
       
	//removed NULL if statement
	//
	tps_queue=queue_create();
	
	return 0;
}

int tps_create(void)
{
	/* TODO: Phase 2 */
	pthread_t tid = pthread_self();
	tps_container_t tps;
//	tps_container_t temp_tps;
	int queue_check;
	int fd = -1;
	off_t offset = 0;

        //check if tps exists
	queue_check=queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);

	//If found return
	if(queue_check == 1)
		return -1;

	//Create Tps
	tps_container_t current_tps = (tps_container_t) malloc(sizeof(tps_container_t));
	current_tps->tid = tid;
	current_tps->memAddress = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_ANONYMOUS, fd, offset);
	current_tps->test = 12;

	queue_enqueue(tps_queue, current_tps);

	return 0;
}

int tps_destroy(void)
{
	/* TODO: Phase 2 */
	tps_container_t tps;
	pthread_t tid = pthread_self();
//	int queue_check;

	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);

	//If not found return
//	if(queue_check == -1)
//		return -1;
	
	//free tps and container
	queue_delete(tps_queue, tps);
	munmap(tps->memAddress, TPS_SIZE);
	free(tps);
	return 0;
}


int tps_read(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	tps_container_t tps;
	pthread_t tid = pthread_self();
//	int queue_check;

	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL)
		return -1;

	memcpy(buffer, (tps->memAddress), length);
	return 0;
	
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	tps_container_t tps;
	pthread_t tid = pthread_self();
	int queue_check;

	queue_check = queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	
	printf("queue_check write: %d\n", queue_check);
	printf("find tid: %d\n", (int)tid);
	printf("tps tid: %d\n", (int)tps->tid);

	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL)
		return -1;


	printf("start memcpy\n");
	memcpy((tps->memAddress), buffer, length);
	printf("finish memcpy\n");

	return 0;
}

int tps_clone(pthread_t tid)
{
	/* TODO: Phase 2 */
	tps_container_t curr_tps, tps;
	pthread_t current_tid = pthread_self();
//	int queue_check1, queue_check2;

	queue_iterate(tps_queue, find_tid, &current_tid, (void**) &curr_tps);
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
//	if(queue_check1!=-1 || queue_check2==-1)
//		return -1;

	memcpy(curr_tps->memAddress, tps->memAddress, TPS_SIZE);

	return 0;
}

