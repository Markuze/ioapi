#ifndef __TRACE_IO_H___
#define __TRACE_IO_H___
/**
	Only safe to use with variables that are located on exclusieve cache lines.
	If ever var is located on shared cache line... well, dont.
	unlike add_return_op the previous value will be added.
*/
#define unsafe_add_return_op(var, val)					\
({									\
	typeof(var) paro_ret__ = val;					\
	switch (sizeof(var)) {						\
	case 1:								\
		asm("xaddb %0, %1\n"					\
			    : "+q" (paro_ret__), "+m" (var)		\
			    : : "memory");				\
		break;							\
	case 2:								\
		asm("xaddw %0, %1"					\
			    : "+r" (paro_ret__), "+m" (var)		\
			    : : "memory");				\
		break;							\
	case 4:								\
		asm("xaddl %0, %1"					\
			    : "+r" (paro_ret__), "+m" (var)		\
			    : : "memory");				\
		break;							\
	case 8:								\
		asm("xaddq %0, %1"					\
			    : "+re" (paro_ret__), "+m" (var)		\
			    : : "memory");				\
		break;							\
	default: __xadd_wrong_size();					\
	}								\
	paro_ret__ += val;						\
	paro_ret__;							\
})


#define unsafe_sub_return(pcp, val)	unsafe_add_return_op(pcp, -(typeof(pcp))(val))

#define TRACE_PAGE_SIZE		(1<<15)//ALIGN(32768, ~PAGE_MASK)
#define TRACE_PAGE_ORDER	get_order(TRACE_PAGE_SIZE)

struct log_page {
	struct log_page *next;
	int len;
	char  bytes[0];
};

void toggle_trace_io(void);
void trace_io_off(void);
void *alloc_trace_bytes(size_t size);
struct log_page *per_cpu_log_page(u16 cpu);
struct log_page *free_per_cpu_log_page(u16 cpu);

struct io_trace_line {
	u64	tsc;
	u64	addr;
	u16	size;
	u16	type;
} __packed;

enum {
	TRACE_IO_ALLOC,
	TRACE_IO_ALLOC_PAGE,
	TRACE_IO_FREE,
	TRACE_IO_TX,
	TRACE_IO_TX_COMP,
	TRACE_IO_RX,
	TRACE_IO_RX_COMP,
};

#include <asm/tsc.h>

#define alloc_io_trace_bytes() alloc_trace_bytes(sizeof(struct io_trace_line))
#define MAX_LOCAL_LOG BIT(14)

static inline void trace_io(void *addr, size_t size, u16 type)
{
	struct io_trace_line *line = alloc_io_trace_bytes();
	if (!line)
		return;
	line->tsc = rdtsc();
	line->addr = (u64)addr;
	line->size = (size >= (1<< 16)) ? 0 : (u16)size;
	line->type = type;
}
#endif /*__TRACE_IO_H___*/
