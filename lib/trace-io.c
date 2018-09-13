#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/trace-io.h>
#include <linux/debugfs.h>

static DEFINE_PER_CPU(struct log_page *, local_log_page);
static DEFINE_PER_CPU(int, local_log_count);
static struct dentry	*trace_dir, *io_toggle;
static u8 logger_active;

void trace_io_off(void)
{
	logger_active = 0;
	trace_printk("trace io is now %s\n", (logger_active) ? "ON" : "OFF");
}
EXPORT_SYMBOL(trace_io_off);

void toggle_trace_io(void)
{
	logger_active ^= 1;
	trace_printk("trace io is now %s\n", (logger_active) ? "ON" : "OFF");
}
EXPORT_SYMBOL(toggle_trace_io);

static inline void alloc_new_trace_page(struct log_page *old)
{
	struct log_page *new_log;
	gfp_t gfp = __GFP_COMP|__GFP_NOWARN|__GFP_NOMEMALLOC|__GFP_ATOMIC;
	struct page *page = alloc_pages_node(numa_mem_id(),
						gfp, TRACE_PAGE_ORDER);

	if (likely(page))
		new_log = page_address(page);

	if (unlikely(this_cpu_cmpxchg(local_log_page,
				old,
				page ? new_log : NULL) != old)) {
		if (page)
			__free_pages(page, TRACE_PAGE_ORDER);
	} else {
		if (likely(page)) {
			new_log->next = old;
			new_log->len = TRACE_PAGE_SIZE - sizeof(struct log_page);
			this_cpu_inc(local_log_count);
		}
	}
	return;
}

void *alloc_trace_bytes(size_t size)
{
	struct log_page *log;
	int len = 0;
	int cnt = this_cpu_read(local_log_count);

	if ((cnt >= MAX_LOCAL_LOG) || logger_active == 0)
		return NULL;
retry:
	log  = this_cpu_read(local_log_page);
	if (unlikely(!log)) {
		alloc_new_trace_page(log);
		log  = this_cpu_read(local_log_page);
		if (unlikely(!log))
			panic("WTF?!? Where's my mem?!");
	}

	len = unsafe_sub_return(log->len, size);

	/* log->len might have changed by softirq */
	if (unlikely(len < sizeof(struct log_page))) {
		alloc_new_trace_page(log);
		goto retry;
	}
	//trace_printk("alloc %p %p [%d] size %ld\n",log, &log->bytes[len], len, size);
	return &log->bytes[len];
}
EXPORT_SYMBOL(alloc_trace_bytes);

struct log_page *per_cpu_log_page(u16 cpu)
{
	return per_cpu(local_log_page, cpu);
}
EXPORT_SYMBOL(per_cpu_log_page);

struct log_page *free_per_cpu_log_page(u16 cpu)
{
	struct log_page *new, *curr = per_cpu(local_log_page, cpu);
	if (!curr)
		return curr;
	new = curr->next;
	__free_pages(virt_to_page(curr), TRACE_PAGE_ORDER);

	return this_cpu_cmpxchg(local_log_page, curr, new);
}
EXPORT_SYMBOL(free_per_cpu_log_page);

static int __init start_trace_if(void)
{
	pr_err("Starting %s\n", __FUNCTION__);
	trace_dir = debugfs_create_dir("trace_io", NULL);
	if (unlikely(!trace_dir))
		goto err;

	io_toggle = debugfs_create_u8("toggle", 0666, trace_dir, &logger_active);
	if (unlikely(!io_toggle))
		goto err;
	return 0;
err:
	trace_printk("Failed to init trace_dir\n");
	return 0;
}
late_initcall(start_trace_if);
