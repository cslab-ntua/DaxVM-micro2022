#define PAGE_SIZE 4096
#define NUM_FILES 10000
#define NUM_THREADS 32

#define READ 1 
#define MMAP 2 
#define MMAP_POPULATE 3 
#define MMAP_DAXVM 4 
#define FLEX_MMAP 5 
#define FLEX_MMAP_POPULATE 6 
#define FLEX_MMAP_DAXVM 7 

void function(char *buf, unsigned long long size);
double read_posix(int fd, unsigned long long size);
void function(char *buf, unsigned long long size);
char *iobuf;
double read_flex(int fd, unsigned long long size);
double read_flex_daxvm(int fd, unsigned long long size);
double read_flex_populate(int fd, unsigned long long size);
double read_mmap(int fd, unsigned long long size);
double read_mmap_populate(int fd, unsigned long long size);
double read_mmap_daxvm(int fd, unsigned long long size);
