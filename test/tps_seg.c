#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>


void* latest_mmap_addr;

void* __real_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);
void* __wrap_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off){
	latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
	return latest_mmap_addr;
}


void my_thread(void){
	tps_create();
	char* tps_addr = latest_mmap_addr;

	tps_addr[0]='\0';
}



int main(int argc, char** argv){
	tps_init(1);
	my_thread();
	return 0;
}




