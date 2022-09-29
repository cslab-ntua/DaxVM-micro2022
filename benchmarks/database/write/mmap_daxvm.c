#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/mman.h>
#include <linux/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../../../daxvm_header.h"
#include <sys/time.h>
#include <string.h>
#include "write.h"

double write_mmap_daxvm(int fd, unsigned long long size, unsigned long long io_size, int repeat, int random){

	unsigned long long start, end;
	double time_taken=0;
	unsigned long long i,j;
	start= _time();
	void *mmap_buf=mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_DAXVM | MAP_SYNC, fd, 0);
	end= _time();
	time_taken+=(end-start);

	if(!random) {	
		for(j=0; j<repeat; j++){
			for (i=0; i<size/io_size; i++){
				start= _time();
				_avx_async_cpy((void*)(((unsigned long long )mmap_buf)+(i*io_size)), (void*) iobuf, io_size);
				end= _time();
				time_taken+=(end-start);
			}
		}
	}
	else {
		struct frand_state s;
		init_rand(&s,1);
		for(unsigned long long j=0; j<repeat*size/io_size; j++){
			unsigned long long i;
			i=rand64_upto(&s,(size/io_size-1));
			start= _time();
			_avx_async_cpy((void*)(((unsigned long long )mmap_buf)+(i*io_size)),(void*)iobuf, io_size);
			end= _time();
			time_taken+=end-start;
		}
	}
	munmap(mmap_buf,size);
	return time_taken;
}

