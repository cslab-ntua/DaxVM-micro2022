# DaxVM: Stressing the Limits of Memory as a File Interface
The implementation of [DaxVM](https://www.cslab.ece.ntua.gr/~xalverti/papers/micro2022_daxvm.pdf), 
our research system, published in the 55th IEEE/ACM International Symposium on Microarchitecture (MICRO) 2022.


### File systems supported
Ext4-Dax, NOVA

### How to use proc configurations (ext4-DAX example, similar for NOVA)
To enable/disable page table support by the file system (build and delete persistent page tables):
* echo 1/0 > /proc/fs/ext4/pmem0/daxvm_page_tables_on

To check how much storage (KB) is occupied by the persistent page tables:
* cat /proc/fs/ext4/pmem0/daxvm_kbytes_occupied

To disable/enable kernel dirty page tracking
* echo 0 > /proc/fs/daxvm/daxvm_msync

To mimic block pre-zeroing (disable synchronous block zeroouts) 
* echo 0 > /proc/fs/ext4/pmem0/daxvm_zeroout_blocks

### How to use the interface 
Include the daxvm_header.h file in your source code, daxvm is currently implementated as a collection of new mmap flags

To enable O(1) mmap -- attachment of the persistent page tables during mmap at 2M granularities:
* Use a special mmap flag (MAP_DAXVM)

To enable asynchronous unmappings
* Use a special mmap flag (MAP_DAXVM_BATHING)

To use the scalable ephemeral heap
* Use a special mmap flag (MAP_DAXVM_EPHEMERAL)

For the nosync mode use the proc interface described above 

For block pre-zeroing use the proc interface described above

### Examples
All paper's microbenchmarks can be found in the benchmarks folder.
All assume that PMem is mounted on /mnt/nvmm1/.
We will make this configurable and also upload our scripts and source code for the real workloads soon.

### What is currently supported
* Pre-populated Page Tables maintained in PMem and Dram (hybrid)
* O(1) mmap (attachment at the PMD level)
* Different permissions per process, enforced at the PMD level always
* Dirty page tracking faults, msync/fsync support
* Asynchronous unmapping
* The ephemeral allocator
* Huge pages
* PT Migration to DRAM when address translation overheads are high 
* Nosync mode -- no kernel dirty page tracking (currently enabled via the proc interface and not with a MAP_NO_MSYNC flag as discussed in the paper)
* Turning block synchronous zeroouts off (to mimic pre-zeroing effect)

### What is missing 
* File system block pre-zeroing routine, we mimic this by disabling synchronous zeroing and with a user-space component (see examples)
* Copy-on-Write support
* Attachment at PUD level 

### File system aging
For file system aging we use [Geriatrix](https://github.com/saurabhkadekodi/geriatrix) and the methodology 
from [WineFS](https://github.com/utsaslab/WineFS). 

### Fixed CPU frequency 
For all our experiments we fix CPU frequency. You can find our script in scripts folder.
* export LD_LIBRARY_PATH=./Linux/tools/power/cpupower
* source ./scripts/setfreq.sh

### Tools and Acknowledgments
For the user-space profiler that triggers page table migrations
to DRAM when address translation overheads are high, we used
the profiler found in [Hawkey](https://github.com/apanwariisc/HawkEye)
and slightly change it.


