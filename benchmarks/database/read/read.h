#define PAGE_SIZE 4096
#define IO_SIZE 2*4096
#define NUM_FILES 10000
#define NUM_THREADS 32

#define READ 0 
#define READ 1 
#define MMAP 2 
#define MMAP_POPULATE 3 
#define MMAP_DAXVM 4 
#define FLEX_MMAP 5 
#define FLEX_MMAP_POPULATE 6 
#define FLEX_MMAP_DAXVM 7 

char *iobuf;
extern int repeat;

double read_posix(int fd, unsigned long long size, int io_size, int repeat, int random);
double read_mmap(int fd, unsigned long long size,int io_size, int repeat, int random);
double read_mmap_populate(int fd, unsigned long long size,int io_size, int repeat, int random);
double read_mmap_daxvm(int fd, unsigned long long size,int io_size, int repeat, int random);
unsigned long long llrand();
unsigned long _time();
void _avx_async_cpy(void *d, const void *s, size_t n);
