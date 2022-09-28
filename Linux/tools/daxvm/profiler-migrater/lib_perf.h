#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <errno.h>
#include <string.h>
#include <asm/unistd.h>

#define SIZE_LONG	8
unsigned long DTLB_LOAD_MISSES_WALK_DURATION = 0;
unsigned long DTLB_STORE_MISSES_WALK_DURATION = 0;
unsigned long CPU_CLK_UNHALTED = 0;
unsigned long DTLB_LOAD_MISSES_WALK_COMPLETED = 0;
unsigned long DTLB_STORE_MISSES_WALK_COMPLETED = 0;

static long perf_event_open(struct perf_event_attr *hw_event,
		pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
						group_fd, flags);
	return ret;
}

int init_perf_event_masks(char *usr)
{
		DTLB_LOAD_MISSES_WALK_DURATION = 0x1531008;
		DTLB_STORE_MISSES_WALK_DURATION = 0x1531049;
		DTLB_LOAD_MISSES_WALK_COMPLETED = 0x530108;
		DTLB_STORE_MISSES_WALK_COMPLETED = 0x530149;
		CPU_CLK_UNHALTED = 0x53003c;
    return 0;
}


static int set_perf_raw_event(struct perf_event_attr *attr, unsigned long event)
{
	memset(attr, 0, sizeof(struct perf_event_attr));
	attr->type = PERF_TYPE_RAW;
	attr->size = sizeof(struct perf_event_attr);
	attr->config = event;
	attr->disabled = 1;
	attr->exclude_kernel = 1;
	attr->exclude_hv = 1;
}

static int set_perf_hw_event_cycles(struct perf_event_attr *attr)
{
	memset(attr, 0, sizeof(struct perf_event_attr));
	attr->type = PERF_TYPE_HARDWARE;
	attr->size = sizeof(struct perf_event_attr);
	attr->config = PERF_COUNT_HW_CPU_CYCLES;
	attr->disabled = 1;
	attr->exclude_kernel = 1;
	attr->exclude_hv = 1;
}

static inline int
get_translation_overhead(int fd_load_walk_duration, int fd_store_walk_duration,
			int fd_unhalted_cycles, int fd_total_cycles)
{
	unsigned long load_walk_duration = 0, store_walk_duration = 0;
	unsigned long unhalted_cycles = 0, total_cycles;
	int ret;

	ret = read(fd_load_walk_duration, &load_walk_duration, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd_store_walk_duration, &store_walk_duration, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd_unhalted_cycles, &unhalted_cycles, SIZE_LONG);
	if (ret != SIZE_LONG || unhalted_cycles == 0)
		goto failure;

	ret = read(fd_total_cycles, &total_cycles, SIZE_LONG);
	if (ret != SIZE_LONG || total_cycles == 0)
		goto failure;

	/* TODO: Check for overflow conditions */
	//return ((load_walk_duration + store_walk_duration) * 100)/ unhalted_cycles;
	return ((load_walk_duration + store_walk_duration) * 100)/ total_cycles;

failure:
	return 0;
}

static inline double
get_cycles_per_walk(int fd_load_walk_duration, int fd_store_walk_duration,
		int fd_load_walk_completed, int fd_store_walk_completed)
{
	unsigned long load_walk_duration = 0, store_walk_duration = 0;
	unsigned long load_walk_completed = 0, store_walk_completed = 0;
	int ret;

	ret = read(fd_load_walk_duration, &load_walk_duration, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd_store_walk_duration, &store_walk_duration, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd_load_walk_completed, &load_walk_completed, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd_store_walk_completed, &store_walk_completed, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	/* TODO: Check for overflow conditions */
	return ((double)(load_walk_duration + store_walk_duration)/
			(load_walk_completed + store_walk_completed));

failure:
	return 0;
}

/*
 * TODO: Error handling for the next 3 functions
 */
static inline void
reset_events(int fd_load, int fd_store, int fd_total,
		int fd_load_completed, int fd_store_completed, int fd_cycles)
{
	ioctl(fd_load, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_store, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_total, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_load_completed, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_store_completed, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_cycles, PERF_EVENT_IOC_RESET, 0);
}

static inline void
enable_events(int fd_load, int fd_store, int fd_total,
		int fd_load_completed, int fd_store_completed, int fd_cycles)
{
	ioctl(fd_load, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_store, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_total, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_load_completed, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_store_completed, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE, 0);
}

static inline void
disable_events(int fd_load, int fd_store, int fd_total,
		int fd_load_completed, int fd_store_completed, int fd_cycles)
{
	ioctl(fd_load, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_store, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_total, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_load_completed, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_store_completed, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE, 0);
}

static inline void
close_files(int fd_load, int fd_store, int fd_total,
		int fd_load_completed, int fd_store_completed, int fd_cycles)
{
	close(fd_load);
	close(fd_store);
	close(fd_total);
	close(fd_load_completed);
	close(fd_store_completed);
	close(fd_cycles);
}

int update_translation_overhead(struct process *proc)
{
/*
 * 	pe_load  ---- DTLB LOAD MISSES WALK DURATION
 * 	pe_store ---- DTLB STORE MISSES WALK DURATION
 * 	pe_total ---- UNHALTED CLOCK CYCLES
 * 	fd_load  ---- File descriptor for pe_load
 * 	fd_store ---- File descriptor for pe_store
 * 	fd_total ---- File descriptor for pe_total
 */
	struct perf_event_attr pe_load, pe_store, pe_total, pe_cycles;
	struct perf_event_attr pe_load_completed, pe_store_completed;
	long long count, count2, count3, count4, count5;
	int fd_load = -1, fd_store = -1, fd_total = -1, fd_cycles = -1;
	int fd_load_completed = -1, fd_store_completed = -1;
	int i, pid, overhead, sleep_period = 1;

	pid = proc->pid;
	/* set per event descriptors */
	set_perf_raw_event(&pe_load, DTLB_LOAD_MISSES_WALK_DURATION);
	set_perf_raw_event(&pe_store, DTLB_STORE_MISSES_WALK_DURATION);
	set_perf_raw_event(&pe_total, CPU_CLK_UNHALTED);
	set_perf_raw_event(&pe_load_completed, DTLB_LOAD_MISSES_WALK_COMPLETED);
	set_perf_raw_event(&pe_store_completed, DTLB_STORE_MISSES_WALK_COMPLETED);
	set_perf_hw_event_cycles(&pe_cycles);

	/* open file descriptors for each perf event */
	fd_load = perf_event_open(&pe_load, pid, -1, -1, 0);
	fd_store = perf_event_open(&pe_store, pid, -1, -1, 0);
	fd_total = perf_event_open(&pe_total, pid, -1, -1, 0);
	fd_load_completed = perf_event_open(&pe_load_completed, pid, -1, -1, 0);
	fd_store_completed = perf_event_open(&pe_store_completed, pid, -1, -1, 0);
	fd_cycles = perf_event_open(&pe_cycles, pid, -1, -1, 0);

	/* check for error conditions */
	if (fd_load == -1 || fd_store == -1 || fd_total == -1 ||
		fd_load_completed == -1 || fd_store_completed == -1 ||
		fd_cycles == -1) {
		printf("Failure while opening descriptors: %d %d %d %d %d %d\n",
			fd_load,fd_store,fd_total, fd_load_completed,
			fd_store_completed, fd_cycles);
		goto failure;
	}
	/* reset the counter values before profiling */
	reset_events(fd_load, fd_store, fd_total, fd_load_completed,
					fd_store_completed, fd_cycles);
	enable_events(fd_load, fd_store, fd_total, fd_load_completed,
					fd_store_completed, fd_cycles);

	/* sleep till the sampling period completes */
	sleep(sleep_period);

	/* disable profiling */
	disable_events(fd_load, fd_store, fd_total, fd_load_completed,
					fd_store_completed, fd_cycles);

	/* get the translation overhead from the measured counters */
	/*
	proc->overhead = get_translation_overhead(fd_load, fd_store,
					fd_total,fd_cycles);
	*/
	//proc->overhead = (0.6 * proc->overhead +
	//		0.4 * get_translation_overhead(fd_load, fd_store,fd_total,fd_cycles));
	proc->overhead = get_translation_overhead(fd_load, fd_store,fd_total,fd_cycles);

	proc->cycles_per_walk = get_cycles_per_walk(fd_load, fd_store,
				fd_load_completed, fd_store_completed);
	close_files(fd_load, fd_store, fd_total, fd_load_completed,
						fd_store_completed, fd_cycles);
	return 0;

failure:
	printf("Error while profiling %d\n", pid);
	return -1;
}
