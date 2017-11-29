#ifndef __ALLOC_TRACE_H__
#define __ALLOC_TRACE_H__

#define ORDER_MAX	7	/* 6 + 1 : 64K .. > 4K : 0 .. 6*/
#define IRQ_STATE	2

#define dump_line(fmt, ...)	trace_printk(fmt "\n", ##__VA_ARGS__);

struct dma_cache_alloc_stats {
	uint64_t alloc[ORDER_MAX][IRQ_STATE];
	uint64_t free[ORDER_MAX][IRQ_STATE];
};

enum {
	ALLOC_TRACE_ALLOC,
	ALLOC_TRACE_FREE,
	ALLOC_TRACE_TYPE_MAX = ALLOC_TRACE_FREE,
};

int alloc_trace_init(void);
void alloc_trace_update(int order, uint32_t type);

#endif /* __ALLOC_TRACE_H__*/
