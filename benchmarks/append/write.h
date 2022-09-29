#define PAGE_SIZE 4096
#define IO_SIZE 512*4096
#define NUM_FILES 10000
#define NUM_THREADS 32

#define WRITE_REUSE 0 
#define WRITE_NEW 1 
#define MMAP 2 
#define MMAP_POPULATE 3 
#define MMAP_DAXVM 4 
#define MMAP_DAXVM_DRAM 5 

#define FILE_SIZE 64*1024*1024ULL
char *iobuf;
extern int repeat;

void _avx_async_cpy(void *d, const void *s, size_t n);
double write_posix(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
double write_mmap(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
double write_mmap_populate(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
double write_mmap_daxvm(int fd, unsigned long long size,unsigned long long io_size, int repeat, int random);
unsigned long long llrand();
unsigned long _time();
