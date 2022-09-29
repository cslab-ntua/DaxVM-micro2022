#include <time.h>
#include <stdio.h>
#include <stdlib.h>
unsigned long long _time(){

        struct timespec  tv;
	      clock_gettime(CLOCK_REALTIME, &tv);
        return (tv.tv_sec) * 1000 * 1000 * 1000  + (tv.tv_nsec);
}


