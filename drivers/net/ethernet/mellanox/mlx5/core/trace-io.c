#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include "trace-io.h"

#include <linux/uaccess.h>
#include <linux/cpumask.h>

#define procname "trace-io"
#define TOP	(TRACE_PAGE_SIZE - sizeof(struct log_page))
#define STEP	sizeof(struct io_trace_line)

static DEFINE_PER_CPU(struct page_frag_cache, logger);
static DEFINE_PER_CPU(unsigned long long, logger_cnt);

static inline int dump_per_core_trace(uint16_t cpu, int *cnt, char __user *buf, size_t max)
{
	int max_len = 0, buffers = 0;

	struct log_page *log = per_cpu_log_page(cpu);
	while (log) {
		int i;
		//pr_err("cpu %d log %p\n", cpu, log);
		if (max < *cnt + 50 * ((TOP-log->len)/STEP) )
			break;
		for (i = log->len; i < TOP; i+=  STEP) {
			char line[64];
			int len;
			struct io_trace_line *trace = (struct io_trace_line *)&log->bytes[i];
			len = snprintf(line, 256, "<%llx: %llx [%d] t %d>\n",
					trace->tsc, trace->addr, trace->size, trace->type);
			max_len = (max_len < len) ? len : max_len;
			if (max <= *cnt +len)
				break;
			*cnt += len;
			copy_to_user(buf + *cnt, line, len);
			/* for correctness:
				1. block new traces
				2. move len up and free exhausted pages
			*/
		}
		//free_per_cpu_log_page(cpu);
		++buffers;
		log->len = TOP;
		log = log->next;
	}
	trace_printk("max len = %u [%lu] buffers %d\n", max_len, (TOP/STEP * max_len), buffers);
	return !!max_len;
}

static ssize_t traceio_write(struct file *file, const char __user *buf,
                              size_t len, loff_t *ppos)
{
	toggle_trace_io();
	return len;
}

static ssize_t traceio_read(struct file *file, char __user *buf,
                             size_t buflen, loff_t *ppos)
{
	int cpu, cnt = 0, out = 0;
	if (!buf)
		return -EINVAL;
#if 0
        /*Stats: will always print something,
         * cat seems to call read again if prev call was != 0
         * ppos points to file->f_pos*/
        if ((*ppos)++ & 1)
                return 0;
#endif
	for_each_online_cpu(cpu) {
		char line[256];
		int len;

		len = snprintf(line, 256, "online cpu %d\n", cpu);
		if (buflen <= cnt + len + 50 * (TOP/STEP) ) {
			trace_printk("<%d>check failed...\n", cpu);
			break;
		}
		cnt += len;
		copy_to_user(buf + cnt, line, len);

		trace_printk("online cpu %d\n", cpu);
		out |= dump_per_core_trace(cpu, &cnt, buf, buflen);
	}

	return out * cnt;
}

int noop_open(struct inode *inode, struct file *file) {return 0;}

static const struct file_operations traceio_fops = {
        .owner   = THIS_MODULE,
        .open    = noop_open,
        .read    = traceio_read,
        .write   = traceio_write,
        .llseek  = noop_llseek,
};

static __init int traceio_init(void)
{
	if (!proc_create(procname, 0666, NULL, &traceio_fops))
		goto err;

	return 0;
err:
	return -1;
}

static __exit void traceio_exit(void)
{
	remove_proc_entry(procname, NULL);
}

