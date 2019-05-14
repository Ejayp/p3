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

queue_t tps_queue;

//data: tps_container_t
//arg: tid to match with
int find_tid(void *data, void *arg){
	tps_container_t a = (tps_container_t) data;
	pthread_t checkTID = (pthread_t) a->tid;
	pthread_t* match = (pthread_t*) arg;

	if(checkTID == *match)
		return 1;
	
	return 0;
}

int find_memAddress(void *data, void *arg){
	tps_container_t a = (tps_container_t) data;
	void* checkAddress = (void*) a->memAddress;
	void** match = (void**) arg;

	if(checkAddress == *match)
		return 1;
	
	return 0;
}

static void segv_handler(int sig, siginfo_t *si, void *context)
{
	/*
	* Get the address corresponding to the beginning of the page where the
	* fault occurred
	*/
	void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));
	tps_container_t tps;

	/*
	* Iterate through all the TPS areas and find if p_fault matches one of them
	*/
	queue_iterate(tps_queue, find_tid, &p_fault, (void**) &tps);

	/* Printf the following error message if match*/
	if (tps)
		fprintf(stderr, "TPS protection error!\n");

	/* In any case, restore the default signal handlers */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	/* And transmit the signal again in order to cause the program to crash */
	raise(sig);
}


int tps_init(int segv)
{
	/* TODO: Phase 2 */

	tps_queue=queue_create();
	
	if(segv){
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
	}	

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
	queue_check = queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);

	//If found return
	if(queue_check == 1)
		return -1;

	//Create Tps
	tps_container_t current_tps = (tps_container_t) malloc(sizeof(tps_container_t));
	current_tps->tid = tid;
	current_tps->memAddress = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, fd, offset);

	if(current_tps->memAddress == MAP_FAILED)
		printf("failed\n");

	enter_critical_section();
	queue_enqueue(tps_queue, current_tps);
	exit_critical_section();

	return 0;
}

int tps_destroy(void)
{
	/* TODO: Phase 2 */
	tps_container_t tps;
	pthread_t tid = pthread_self();

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);

	//If not found return
	if(tps->memAddress == NULL)
		return -1;
	
	//free tps and container
	queue_delete(tps_queue, tps);
	exit_critical_section();

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

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL || tps->memAddress == NULL)
		return -1;

	//Enables reading
	mprotect(tps->memAddress, length, PROT_READ);
	memcpy(buffer, tps->memAddress, length);
	return 0;
	
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	tps_container_t tps;
	pthread_t tid = pthread_self();

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL || tps->memAddress == NULL)
		return -1;

	//Enables writing
	mprotect(tps->memAddress, length, PROT_WRITE);
	memcpy(tps->memAddress, buffer, length);
	return 0;
}

int tps_clone(pthread_t tid)
{
	/* TODO: Phase 2 */
	tps_container_t curr_tps, tps;
	pthread_t current_tid = pthread_self();
	int fd = -1;
	off_t offset = 0;

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &current_tid, (void**) &curr_tps);
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();

	//Error Management
	if(tps->memAddress == NULL)
	{
		return -1;
	}
	//Maps memory to current tps and copies tps from @tid
	curr_tps = (tps_container_t) malloc(sizeof(tps_container_t));
	curr_tps->tid = current_tid;
	curr_tps->memAddress = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, fd, offset);
	memcpy(curr_tps->memAddress, tps->memAddress, TPS_SIZE);
	mprotect(tps->memAddress, TPS_SIZE, PROT_NONE);
	
	//then enqueues
	enter_critical_section();
	queue_enqueue(tps_queue, curr_tps);
	exit_critical_section();
	return 0;
}

