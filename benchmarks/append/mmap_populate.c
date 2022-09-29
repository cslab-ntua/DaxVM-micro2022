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

double write_mmap_populate(int fd, unsigned long long size, unsigned long long io_size, int repeat, int random){

	unsigned long start, end;
	double time_taken=0;
	char fname[512];
	sprintf(fname, "/mnt/nvmm1/log.txt");
	fd = open(fname, O_CREAT | O_RDWR, 0666);
	
	start = _time();
	fallocate(fd, 0, 0, size);
	void *mmap_buf=mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
  end=_time();
	time_taken+=end-start;

	for (unsigned long long i=0; i<size/io_size; i++){
		start = _time();
		_avx_async_cpy((void*)(((unsigned long long )mmap_buf)+(i*io_size)), (void*) iobuf, io_size);
    end=_time();
		time_taken+=end-start;
	}

	start = _time();
	munmap(mmap_buf,size);
  end=_time();
	time_taken+=end-start;
  close(fd);
	return time_taken;
}



