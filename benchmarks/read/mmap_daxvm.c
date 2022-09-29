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
#include "read.h"

double read_mmap_daxvm(int fd, unsigned long long size){

	double time_taken=0;
	double start = _time();
	char *mmap_buf=mmap(NULL, size, PROT_READ, MAP_SHARED | MAP_DAXVM, fd, 0);
	function(mmap_buf, size);
	munmap(mmap_buf,size);
  double end=_time();
	time_taken+=end-start;
	return time_taken;
}


