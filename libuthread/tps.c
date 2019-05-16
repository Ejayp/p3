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

//data structure to contain thread's tid and location of tps page
struct tps_container {
	pthread_t tid;
	pages_t page;
};

//data structure to contain the tps page and reference counter
struct pages {
	char* memAddress;
	int refCounter;
};

//global queue to contain all the tps and tid
queue_t tps_queue;

//data: tps_container_t
//arg: tid to match with
//return 1 if current thread's tid matches a tid in the queue
int find_tid(void *data, void *arg){
	tps_container_t a = (tps_container_t) data;
	pthread_t checkTID = (pthread_t) a->tid;
	pthread_t* match = (pthread_t*) arg;

	//return 1 if there is a match
	if(checkTID == *match)
		return 1;
	
	return 0;
}

//return 1 if memory thread's memory address matches an address in the queue
int find_memAddress(void *data, void *arg){
	tps_container_t a = (tps_container_t) data;
	char* checkAddress = (char*) a->page->memAddress;
	char* match = (char*) arg;

	//return 1 if there is a match
	if(checkAddress == match)
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

	printf("TPS protection error!\n");
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
	//initialize the queue
	tps_queue=queue_create();
	
	//initialize segv and segv handler
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
	current_tps->tid = tid; //set current thread's tid to associate it with tps
	page->memAddress = mmap(NULL, 
				TPS_SIZE, 
				PROT_NONE, 
				MAP_SHARED | MAP_ANONYMOUS, 
				fd, 
				offset);
	page->refCounter = 1; //only 1 thread for a newly created tps

	//if mmap fails return -1
	if(page->memAddress == MAP_FAILED)
		return -1;

	//Sets page to the current tps and then enqueues the tps
	current_tps->page = page;
	enter_critical_section(); //critical section (global variable)
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

	//decrements page counter
	tps->page->refCounter--;

	//if its the only tps on a page then unmap it
	if(tps->page->refCounter == 0)
		munmap(tps->page->memAddress, TPS_SIZE);

	//dissasociate the page from tps then frees the entire struct
	tps->page = NULL;
	exit_critical_section();
	free(tps);
	return 0;
}


int tps_read(size_t offset, size_t length, char *buffer)
{
	tps_container_t tps = NULL;
	pthread_t tid = pthread_self();
	int i;

	enter_critical_section();
	queue_iterate(tps_queue, find_tid, &tid, (void**) &tps);
	exit_critical_section();
	
	//error management
	//reading offbounds or tps not found or buffer is NULL
	if(offset+length>TPS_SIZE || buffer==NULL || tps == NULL)
		return -1;

	//Enables reading
	mprotect(tps->page->memAddress, TPS_SIZE, PROT_READ);

	//copy contents in memAddress into buffer
	for(i = 0; i < length; i++)
		buffer[i] =  tps->page->memAddress[offset + i];

	//reset protection to no access
	mprotect(tps->page->memAddress, TPS_SIZE, PROT_NONE);
	return 0;
}

int tps_write(size_t offset, size_t length, char *buffer)
{
	tps_container_t tps = NULL;
	pages_t newPage;
	pthread_t tid = pthread_self();
	int fd = -1;
	off_t off = 0;
	int i;

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
		newPage->memAddress = mmap(NULL, 
					TPS_SIZE, 
					PROT_READ | PROT_WRITE, 
					MAP_SHARED | MAP_ANONYMOUS, 
					fd, 
					off);
		mprotect(tps->page->memAddress, TPS_SIZE, PROT_READ);
		memcpy(newPage->memAddress, tps->page->memAddress, TPS_SIZE);
		mprotect(tps->page->memAddress, TPS_SIZE, PROT_NONE);
		tps->page = newPage;
	}
	
	//Enables writing and copies buffer
	mprotect(tps->page->memAddress, TPS_SIZE, PROT_WRITE);

	//write contents in buffer into memAddress
	for(i=0;i<length;i++)
		tps->page->memAddress[offset+i]=buffer[i];

	//reset protection to no access
	mprotect(tps->page->memAddress, TPS_SIZE, PROT_NONE);
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
	curr_tps->page = tps->page;
	curr_tps->page->refCounter++;
	
	//then enqueues
	enter_critical_section();
	queue_enqueue(tps_queue, curr_tps);
	exit_critical_section();
	return 0;
}

