#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "/ssd2/ATC2022/header.h"
#include <sys/time.h>
#include <string.h>
#include <immintrin.h>

char *iobuf;

void _avx_async_cpy(void *d, const void *s, size_t n) {
         __m256i *dVec = (__m256i *)(d);
         const __m256i *sVec = (const __m256i *)(s);
         size_t nVec = n / sizeof(__m256i);
         for (; nVec > 0; nVec--, sVec++, dVec++) {
               const __m256i temp = _mm256_load_si256(sVec);
               _mm256_stream_si256(dVec, temp);
         }
         _mm_sfence();
}


void loop(){
	char fname[512];
	unsigned long long size = 64*1024*1024;
	int fd;
	sprintf(fname, "/mnt/nvmm1/zero.txt");
	fd = open(fname, O_CREAT | O_RDWR, 0666);
	fallocate(fd, 0, 0, size);
	void *mmap_buf=mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
	while(1){
		usleep(100*1000);
		//sprintf(fname, "/mnt/nvmm1/zero.txt");
		//fd = open(fname, O_CREAT | O_RDWR, 0666);
		//fallocate(fd, 0, 0, size);
		//void *mmap_buf=mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
		_avx_async_cpy((void*)mmap_buf, (void*) iobuf, size);
		//pwrite(fd, (void*) iobuf, size,0);
		//memcpy((void*)mmap_buf, (void*) iobuf, size);
	}
	unlink("/mnt/nvmm1/zero.txt");
}

void main (){

	char fname[512];
	unsigned long long size = 64*1024*1024;
	iobuf=mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	memset(iobuf,0,size);
	//_avx_async_cpy((void*)(((unsigned long long )mmap_buf)+(i*io_size)), (void*) iobuf, io_size);
	loop();
	return;
}

