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

double read_posix(int fd, unsigned long long size){
	int ret;
	double time_taken=0;
	lseek(fd,0,SEEK_SET);
	double start = _time();
	while( (ret = read(fd, iobuf, size))){
		function(iobuf, ret);
	}
  double end=_time();
	time_taken+=end-start;
	return time_taken;
}


