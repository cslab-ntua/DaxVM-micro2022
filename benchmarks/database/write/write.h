#include "types.h"
#include "rand.h"
#define PAGE_SIZE 4096
#define IO_SIZE 2*4096
#define NUM_FILES 10000
#define NUM_THREADS 32

#define WRITE 1 
#define MMAP  2 
#define MMAP_POPULATE 3 
#define MMAP_DAXVM 4 
#define MMAP_DAXVM_DRAM 5 

char *iobuf;
extern int repeat;

unsigned long long _time();
void _avx_async_cpy(void *d, const void *s, size_t n);
double write_posix(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
double write_mmap(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
double write_mmap_populate(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
double write_mmap_daxvm(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
unsigned long long llrand();
