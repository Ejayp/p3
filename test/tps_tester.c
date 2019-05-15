#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

static char msg1[TPS_SIZE] = "Hello world!\n";
static char msg2[TPS_SIZE] = "hello world!\n";
static char msg3[TPS_SIZE] = "Hello world!hello world!\n";

static sem_t sem1, sem2;

void *thread2(void* arg)
{
	char *buffer = malloc(TPS_SIZE);
	char* nullBuffer=NULL;
	int tester;

	/* Test creation of mulitple tps for same thread */
	tester=tps_create();
	assert(tester==0);
	tester=tps_create();
	assert(tester==-1);
	printf("completed test on tps_create()\n");

	/* Testing write edge cases */
	tester=tps_write(0, TPS_SIZE, nullBuffer); //null buffer
	assert(tester==-1);
	tester=tps_write(10, TPS_SIZE, msg1); //offset+length > tps_size
	assert(tester==-1);

	/*Valid tps_write*/
	tester=tps_write(0, TPS_SIZE, msg1);
	assert(tester==0);
	printf("completed test on tps_write()\n");
	
	/* Testing read edge cases */
	tester=tps_read(0, TPS_SIZE, nullBuffer); //null buffer
	assert(tester==-1);
	memset(buffer, 0, TPS_SIZE);
	tester=tps_read(10, TPS_SIZE, buffer); //offset+length > tps_size
	assert(tester==-1);

	/* Read from TPS and make sure it contains the message */
	memset(buffer, 0, TPS_SIZE);
	tester=tps_read(0, TPS_SIZE, buffer);
	assert(tester==0);
	printf("completed test on tps_read()\n");

	/* Transfer CPU to thread 1 and get blocked */
	sem_up(sem1);
	sem_down(sem2);

	/* Destroy TPS and quit */
	tps_destroy();
	return NULL;
}

void *thread1(void* arg)
{
	pthread_t tid;
	char *buffer = malloc(TPS_SIZE);
	int tester;
	off_t offset = 12;

	/* Create thread 2 and get blocked */
	pthread_create(&tid, NULL, thread2, NULL);
	sem_down(sem1);

	/* Testing multiple cloning */
	tester=tps_clone(tid);
	assert(tester==0);
	tester=tps_clone(tid);
	assert(tester==-1);
	printf("completed test on tps_clone()\n");

	/* Testing offset write */
	memset(buffer, 0, TPS_SIZE);
	tps_write(offset, TPS_SIZE - offset, msg2);
	tps_read(0, TPS_SIZE, buffer);
	assert(!strcmp(msg3, buffer));
	printf("completed test on offset writing\n");

	/* Clears TPS data*/
	memset(buffer, 0, TPS_SIZE);
	tps_write(0, TPS_SIZE, buffer);

	/* Testing write and read offset */
	tps_write(offset, TPS_SIZE - offset, msg2);
	tps_read(offset, TPS_SIZE - offset, buffer);
	assert(!strcmp(msg2, buffer));
	printf("completed test on offset read and write\n");


	/* Transfer CPU to thread 2 */
	sem_up(sem2);

	/* Wait for thread2 to die, and quit */
	pthread_join(tid, NULL);
	tester=tps_destroy();
	assert(tester==0);	
	tester=tps_destroy(); //destroy twice
	assert(tester==-1);
	printf("completed test on tps_destroy\n");

	return NULL;
}

int main(int argc, char **argv)
{
	pthread_t tid;

	/* Create two semaphores for thread synchro */
	sem1 = sem_create(0);
	sem2 = sem_create(0);

	/* Init TPS API */
	tps_init(1);

	/* Create thread 1 and wait */
	pthread_create(&tid, NULL, thread1, NULL);
	pthread_join(tid, NULL);

	/* Destroy resources and quit */
	sem_destroy(sem1);
	sem_destroy(sem2);
	return 0;
}
