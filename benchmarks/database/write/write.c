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
#include "write.h"


double write_posix(int fd, unsigned long long size, unsigned long long io_size, int repeat, int random){

	unsigned long long ret=0;
	unsigned long start, end;
	double time_taken=0;
	if(!random) {

		for(int j=0; j<repeat; j++){
			for (unsigned long long i=0; i<size/io_size; i++){
				ret = 0;
				start= _time();
				do {
					ret += pwrite(fd, iobuf, io_size-ret, i*io_size+ret);
				}while(ret<io_size);
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
			ret=0;
			start= _time();
			do {
				ret += pwrite(fd, iobuf, io_size-ret, i*io_size+ret);
			}while(ret<io_size);
			end= _time();
			time_taken+=end-start;
		}
	}
	return time_taken;
}

