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

	unsigned long long ret;
	unsigned long start, end;
	double time_taken=0;
	char fname[512];
	sprintf(fname, "/mnt/nvmm1/log.txt");
	fd = open(fname, O_CREAT | O_RDWR, 0666);
	for (unsigned long long i=0; i<size/io_size; i++){
		start = _time();
		ret = 0;
		do {
			ret += pwrite(fd, iobuf+ret, io_size-ret, i*io_size+ret);
		}while(ret<io_size);
        	end=_time();
		time_taken+=end-start;
	}
  close(fd);
	return time_taken;
}

