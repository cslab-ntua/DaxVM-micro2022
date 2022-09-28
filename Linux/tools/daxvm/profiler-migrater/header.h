#define FILENAMELENGTH		50
#define LINELENGTH		2000
#define PAGE_SIZE		4096
/* For a process to be eligible for THP considerations,
 * it must have anon memory size of at least this value.
 * Its current value is 300MB (based on heuristics).
 */
#define	ELIGIBILITY_THRESHOLD	1000000
#define	IS_CONSIDERABLE		1

/*
 * skip		This is an optional field to ignore processes
 * 		that should not be considered for THP. An example usage
 * 		can be small memory processes (e.g. < 200MB ). It can be
 * 		used to filter out background services (if any).
 *		Currently, we are seeing that some processes do not have an
 *		entry in /proc. We can't do anything about such processes at this
 *		point. However, it is still being kept aroud just in case a proc
 * 		entry appears for it later.
 * found	Denotes whether the process still exists.
 */
struct process {
	bool skip;
	int pid;
	double overhead;
	unsigned long timestamp;
	double cycles_per_walk;
	struct process *next;
};
