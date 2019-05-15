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
	pages_t page;
};

struct pages {
	void* memAddress;
	int refCounter;
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
	void* checkAddress = (void*) a->page->memAddress;
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
	tps_container_t tps = NULL;

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
	pthread_t tid = pthread_self();
	tps_container_t tps = NULL;
	pages_t page;
	int fd = -1;
	off_t offset = 0;

	//check if tps exists
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);

	//If found return
	if(tps != NULL)
		return -1;

	//Create Tps and its page
	tps_container_t current_tps = (tps_container_t) malloc(sizeof(tps_container_t));
	page = (pages_t) malloc(sizeof(pages_t));
	current_tps->tid = tid;
	page->memAddress = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, fd, offset);
	page->refCounter = 1;

	//if mmap fails return -1
	if(page->memAddress == MAP_FAILED)
		return -1;

	//Sets page to the current tps and then enqueues the tps
	current_tps->page = page;
	enter_critical_section();
	queue_enqueue(tps_queue, current_tps);
	exit_critical_section();

	return 0;
}

int tps_destroy(void)
{
	tps_container_t tps = NULL;
	pthread_t tid = pthread_self();

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();

	//If not found return
	if(tps == NULL)
		return -1;
	
	//free tps and container
	enter_critical_section();
	queue_delete(tps_queue, tps);
	exit_critical_section();

	//if its the only tps on a page then unmap it
	if(tps->page->refCounter == 1)
		munmap(tps->page->memAddress, TPS_SIZE);

	free(tps);
	return 0;
}


int tps_read(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	tps_container_t tps = NULL;
	pthread_t tid = pthread_self();

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL || tps == NULL)
		return -1;

	//Enables reading
	mprotect(tps->page->memAddress, length, PROT_READ);
	memcpy(buffer, tps->page->memAddress, length);
	return 0;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	/* TODO: Phase 2 */
	tps_container_t tps = NULL;
	pages_t newPage;
	pthread_t tid = pthread_self();
	int fd = -1;
	off_t off = 0;

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL || tps == NULL)
		return -1;

	//if refCounter has more than one tps
	//then create a new page that copies the old pages contents
	//set new page to tps's page
	if(tps->page->refCounter > 1){
		tps->page->refCounter--;
		newPage = (pages_t) malloc(sizeof(pages_t));
		newPage->memAddress = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, fd, off);
		memcpy(newPage->memAddress, tps->page->memAddress, TPS_SIZE);
		tps->page = newPage;
		mprotect(tps->page->memAddress, length, PROT_NONE);
	}
	
	//Enables writing and copies buffer
	mprotect(tps->page->memAddress, length, PROT_WRITE);
	memcpy(tps->page->memAddress, buffer, length);
	return 0;
}

int tps_clone(pthread_t tid)
{
	tps_container_t curr_tps = NULL;
	tps_container_t tps = NULL;
	pthread_t current_tid = pthread_self();

	//finds current and other thread in the queue
	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &current_tid, (void**) &curr_tps);
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();

	//Fails if current thread has a tps or other thread doesnt have a tps
	if(tps == NULL || curr_tps != NULL)
		return -1;

	//makes curr_tps and assigns its page to the other threads page
	curr_tps = (tps_container_t) malloc(sizeof(tps_container_t));
	curr_tps->tid = current_tid;
	curr_tps->page = (pages_t) malloc(sizeof(pages_t));
	mprotect(tps->page->memAddress, TPS_SIZE, PROT_READ | PROT_WRITE);
	curr_tps->page = tps->page;
	curr_tps->page->refCounter++;
	mprotect(tps->page->memAddress, TPS_SIZE, PROT_NONE);
	
	//then enqueues
	enter_critical_section();
	queue_enqueue(tps_queue, curr_tps);
	exit_critical_section();
	return 0;
}

