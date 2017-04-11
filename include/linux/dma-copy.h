#ifndef DMA_CHOPY_H
#define DMA_CHOPY_H

#include <linux/threads.h>

#define MAX_COMPOUND_SHADOW_PER_NODE	(16 * 1024)
#define COPY_CORES			NR_CPUS
#define COPY_HASHES			(COPY_CORES * 2)
#define MIN_COPY_ALLOC_SZ		64
#define MIN_COPY_ALLOC_MASK		(MIN_COPY_ALLOC_SZ -1)
//magazine of shadow_entry array.
struct shadow_entry {
	void *shadow;
	void *real;
};

struct compound_shadow {
	struct shadow_entry entry[512] ;
};

struct copy_mdata {
	struct dma_map_ops *orig_dma_ops;
	struct dma_map_ops *dma_ops;
	struct compound_shadow **compound_entry[COPY_CORES * 2]; //tx and rx
};
//[MAX_COMPOUND_SHADOW_PER_NODE]

// ~128MB  - can decreasey dramatically by allocating compound_shadow_from mag.
//
/////////////////////////////////////////////////////////

struct device;
int dma_copy_register_dev(struct device *dev);

#endif /*DM_COPY_H*/
