#include "linux/kernel.h"
#include "linux/debugfs.h"
#include "linux/preempt.h"
#include "linux/alloc_trace.h"

static DEFINE_PER_CPU(struct dma_cache_alloc_stats, alloc_stat);
struct dentry *dir, *file;

#define SNPRINTF_BUFF_SZ 16

static void update_alloc_stat(struct dma_cache_alloc_stats *stats,
				struct dma_cache_alloc_stats *update)
{
	int i;

	for (i = 0; i < ORDER_MAX; i++) {
		stats->alloc[i][0] += update->alloc[i][0];
		stats->alloc[i][1] += update->alloc[i][1];
		stats->free[i][0] += update->free[i][0];
		stats->free[i][1] += update->free[i][1];
	}
}

static void dump_alloc_stat(struct dma_cache_alloc_stats *stats, const char* str)
{
	int i;

	for (i = 0; i < ORDER_MAX; i++) {
		dump_line("%-6s: alloc order %-2d\t %-10llu %-10llu : %-11llu",
				str, i,
				stats->alloc[i][0], stats->alloc[i][1],
			 	stats->alloc[i][0] + stats->alloc[i][1]);
		dump_line("%-6s: free  order %-2d\t %-10llu %-10llu : %-11llu",
				str, i,
				stats->free[i][0], stats->free[i][1],
			 	stats->free[i][0] + stats->free[i][1]);
	}
}

static int debugfs_alloc_trace_set(void)
{
	struct dma_cache_alloc_stats total_stats = {0};
	char buf[SNPRINTF_BUFF_SZ] = {0};
	int cpu;

	for_each_possible_cpu(cpu) {
		struct dma_cache_alloc_stats *cpu_stat = per_cpu_ptr(&alloc_stat, cpu);
		snprintf(buf, SNPRINTF_BUFF_SZ, "cpu %-2d:", cpu);
		dump_alloc_stat(cpu_stat, buf);
		update_alloc_stat(&total_stats, cpu_stat);
	}
	dump_alloc_stat(&total_stats, "Total");
	return 0;
}

static int ta_dump_show(struct seq_file *m, void *v)
{
	return debugfs_alloc_trace_set();
}

static int ta_dump_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, ta_dump_show, NULL);
}

static const struct file_operations ta_dump_fops = {
	.owner		= THIS_MODULE,
	.open		= ta_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int alloc_trace_init(void)
{
	/* Double init*/
	if (dir)
		return -EEXIST;

	dir = debugfs_create_dir("alloc_trace", NULL);
	if (unlikely(!dir))
		return PTR_ERR(dir);

	file = debugfs_create_file("get_stats", 0666, dir, NULL /* no specific val for this file */,
				&ta_dump_fops);
	return 0;
}
EXPORT_SYMBOL(alloc_trace_init);

/* update func : order, op {alloc/delete}*/

void alloc_trace_update(int order, uint32_t type)
{
	struct dma_cache_alloc_stats *cpu_stat = this_cpu_ptr(&alloc_stat);

	if (unlikely(type > ALLOC_TRACE_TYPE_MAX))
		panic("cant update with type %d\n", type);

	if (type == ALLOC_TRACE_ALLOC)
		++cpu_stat->alloc[order][in_softirq()]; /*WARNING: Change this when porting to vanilla */
	if (type == ALLOC_TRACE_FREE)
		++cpu_stat->free[order][in_softirq()]; /*WARNING: Change this when porting to vanilla */
}
EXPORT_SYMBOL(alloc_trace_update);
