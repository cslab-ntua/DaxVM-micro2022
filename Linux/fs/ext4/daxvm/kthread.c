#include "daxvm.h"
struct prezero {
	atomic64_t size;
	atomic64_t sa;
}

static int __init create_tlb_single_page_flush_ceiling(void)
{
        return 0;
}
late_initcall(create_tlb_single_page_flush_ceiling);

