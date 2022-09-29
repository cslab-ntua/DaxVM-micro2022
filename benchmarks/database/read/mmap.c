#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
#include "read.h"
#include "rand.h"

double read_mmap(int fd, unsigned long long size, int io_size, int repeat, int random){

	unsigned long start, end;
	double time_taken=0;
	start= _time();
	void *mmap_buf=mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	end= _time();
	time_taken+=end-start;
	if(!random) {	
		for(int j=0; j<repeat; j++){
			for (unsigned long long i=0; i<size/io_size; i++){
				start= _time();
				memcpy((void*)iobuf, (void*)(((unsigned long long )mmap_buf)+(i*io_size)), io_size);
				end= _time();
				time_taken+=end-start;
			}
		}
	}
	else {
		struct frand_state s;
		init_rand(&s,1);
		for(int j=0; j<repeat*size/io_size; j++){
			unsigned long long i;
			i=rand64_upto(&s,(size/io_size-1));
			start= _time();
			if(((unsigned long long)mmap_buf+i*io_size)%1024) printf("Fuck!\n");
		  memcpy((void*)iobuf, (void*)(((unsigned long long )mmap_buf)+(i*io_size)), io_size);
			end= _time();
			time_taken+=end-start;
		}
	}
	start= _time();
	munmap(mmap_buf,size);
	end= _time();
	time_taken+=end-start;
	return time_taken;
}

