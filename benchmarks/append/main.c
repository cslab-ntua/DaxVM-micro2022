#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../../daxvm_header.h"
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include "write.h"

int repeat=1;

struct info {
  int tid;
	int repeat;
  int type;
  int random;
	unsigned long long iosize;
  int num_threads;
  unsigned long long size;
};

unsigned long _time(){
  struct timeval  tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec) * 1000 * 1000 + (tv.tv_usec);
}

void write_mmap_initial(int fd, unsigned long long size){
	char *mmap_buf=mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	memset(mmap_buf, 'c', size);
	munmap(mmap_buf,size);
}

unsigned long long llrand() {
    unsigned long long r = 0;

    for (int i = 0; i < 5; ++i) {
        r = (r << 15) | (rand() & 0x7FFF);
    }

    return r & 0xFFFFFFFFFFFFFFFFULL;
}

double WriteFiles(void *t){
	int type = ((struct info *)t)->type;
  int threadid = ((struct info *)t)->tid;
  int num_threads = ((struct info *)t)->num_threads;
  unsigned long long size = ((struct info *)t)->size;
	int fd;
  char fname[512];

	int repeat = ((struct info *)t)->repeat;
	unsigned long long iosize = ((struct info *)t)->iosize;
	int random = ((struct info *)t)->random;

	double time=0;
	for(unsigned long i=0; i<repeat; i++) {
		unlink("/mnt/nvmm1/log.txt");
		switch(type){

			case WRITE_NEW:
				time += write_posix(fd, size, iosize, repeat, random);	
				break;
		
			case WRITE_REUSE:
				time += write_posix(fd, size, iosize, repeat, random);	
				break;

			case MMAP:
				time += write_mmap(fd, size, iosize, repeat, random);	
				break;

			case MMAP_POPULATE:
				time += write_mmap_populate(fd, size, iosize, repeat, random);	
				break;

			case MMAP_DAXVM:
				time += write_mmap_daxvm(fd, size, iosize, repeat, random);	
				break;

			default:
				printf("Something is wrong!\n");	
		}
	}
	return time;
}

void main(int argc, char *argv[]){
  char fname[512];
  pthread_t threads[NUM_THREADS];
  struct info inf[NUM_THREADS];
  int num_threads = atoi(argv[1]);
	unsigned long start, end;
	double time_taken;
	int fd, t, rc;
	unsigned long num_files;

	unsigned long long size = atoll(argv[3]);
	unsigned long long iosize = size;
	int repeat=atoi(argv[4]);
	int random=atoi(argv[5]);

	iobuf = (char*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE|MAP_PRIVATE, -1, 0); 
	memset(iobuf, 'c', size);	
	if(num_threads > 1) {
        	for(t=0;t<num_threads;t++){
                	inf[t].tid = t;
                	inf[t].type = atoi(argv[2]);
                	inf[t].num_threads = num_threads;
                	inf[t].size = size;
                	inf[t].repeat = repeat;
                	inf[t].iosize = iosize;
                	inf[t].random = random;
                	rc = pthread_create(&threads[t], NULL, WriteFiles, &inf[t]);
        	}
        	for(t=0;t<num_threads;t++){
                	pthread_join(threads[t],NULL);
        	}
	}
	else {
        inf[0].tid = 0;
        inf[0].type = atoi(argv[2]);
        inf[0].num_threads = num_threads;
        inf[0].size = size;
        inf[0].repeat = repeat;
        inf[0].iosize = iosize;
        inf[0].random = random;
		    time_taken = WriteFiles(&inf[0]);
	}	
        time_taken = time_taken/1000000;
	
        printf("Throughput: %f\n", ((repeat*size)/1024/1024)/time_taken);
        printf("Time taken: %f\n", time_taken);
}

