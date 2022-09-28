# DaxVM: A Fast and Scalable interface to persistent data

### File systems supported
Ext4-Dax

### How to use
Currently to control the page table support by the file system (build and delete persistent page tables):
* echo 1/0 > /proc/fs/ext4/pmem0/daxvm_page_tables_on

To check how much storage (KB) is occupied by the persistent page tables:
* cat /proc/fs/ext4/pmem0/daxvm_kbytes_occupied

To enable O(1) mmap -- attachment of the persistent page tables during mmap at 2M granularities:
* Use a special mmap flag (MAP_DAXVM)
* Include the tools/daxvm/daxvm_header.h file in your source code 

### Examples
Some microbenchmarks and the ag and filebench workloads
using the DaxVM interface can be found under tools/daxvm.

### What is currently supported
* Persistent Page Tables (stored in PMem)
* O(1) mmap attaching ppt at the PMD level
* Different permissions per process, enforced at the PMD level always
* Dirty page tracking faults, Page Cache metadata support, msync/fsync support

### What is missing and I will push in a week
* Huge Pages
* Page Tables maintained in Dram (hybrid)
* Copy-on-Write support
* Fast munmap (batching)
* The ephemeral allocator

### What will not be supported unless you need it 
* Attachment at PUD level 
* mprotect, madvise, mremap

