#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "write.h"

#define FILE_SIZE 100*1024*1024*1024ULL

int repeat=1;

struct info {
  int tid;
	int repeat;
  int type;
  int random;
	unsigned long long io_size;
  int num_threads;
  unsigned long long size;
};

unsigned long long llrand() {
    unsigned long long r = 0;

    for (int i = 0; i < 5; ++i) {
        r = (r << 15) | (rand() & 0x7FFF);
    }

    return r & 0xFFFFFFFFFFFFFFFFULL;
}

double ReadFiles(void *t){
	int type = ((struct info *)t)->type;
  int threadid = ((struct info *)t)->tid;
  int num_threads = ((struct info *)t)->num_threads;
  unsigned long long size = ((struct info *)t)->size;
	int fd;
  char fname[512];

	int repeat = ((struct info *)t)->repeat;
	unsigned long long io_size = ((struct info *)t)->io_size;
	int random = ((struct info *)t)->random;

	sprintf(fname, "/mnt/nvmm1/micro/large_test.txt");
	if( access( fname, F_OK ) == 0 ) {
		fd = open(fname, O_RDWR, 0666);
	} else {
		return -1;
	}	

	double time;
	switch(type){

		case WRITE:
			time = write_posix(fd, size, io_size, repeat, random);	
			break;
		
		case MMAP:
			time = write_mmap(fd, size, io_size, repeat, random);	
			break;

		case MMAP_POPULATE:
			time = write_mmap_populate(fd, size, io_size, repeat, random);	
			break;

		case MMAP_DAXVM:
			time = write_mmap_daxvm(fd, size, io_size, repeat, random);	
			break;

		default:
			printf("Something is wrong!\n");	
	}
	close(fd);
	return time;
}

void main(int argc, char *argv[]){
  char fname[512];
  pthread_t threads[NUM_THREADS];
  struct info inf[NUM_THREADS];
	unsigned long long size = FILE_SIZE;
  int num_threads = atoi(argv[1]);
	unsigned long start, end;
	double time_taken;
	int fd, t, rc;
	unsigned long num_files;

	unsigned long long io_size = atol(argv[3]);
	int repeat=atoi(argv[4]);
	int random=atoi(argv[5]);
	time_taken = 0;

	iobuf = (char*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_POPULATE|MAP_PRIVATE, -1, 0); 
	memset(iobuf, 'c', size);
	sprintf(fname, "/mnt/nvmm1/micro/large_test.txt");
	fd = open(fname, O_CREAT | O_RDWR, 0666);
	if (lseek(fd,0,SEEK_END) < size){
		lseek(fd,0,SEEK_SET);
		write_posix(fd, size, size,1,0);
    fsync(fd);    
	}
	close(fd);	
	if(num_threads > 1) {
        	for(t=0;t<num_threads;t++){
                	inf[t].tid = t;
                	inf[t].type = atoi(argv[2]);
                	inf[t].num_threads = num_threads;
                	inf[t].size = size;
                	inf[t].repeat = repeat;
                	inf[t].io_size = io_size;
                	inf[t].random = random;
                	rc = pthread_create(&threads[t], NULL, ReadFiles, &inf[t]);
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
        inf[0].io_size = io_size;
        inf[0].random = random;
		    time_taken = ReadFiles(&inf[0]);
	}	
  time_taken = time_taken/1000000000;
  printf("Throughput: %f\n", ((repeat*size)/1024/1024)/time_taken);
  printf("Time taken: %f\n", time_taken);
}

