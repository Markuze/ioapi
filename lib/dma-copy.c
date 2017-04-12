#include <linux/export.h>
#include <linux/dma-copy.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <asm/page.h>

#ifndef assert
#define assert(expr) 	do { \
				if (unlikely(!(expr))) { \
					trace_printk("Assertion failed! %s, %s, %s, line %d\n", \
						   #expr, __FILE__, __func__, __LINE__); \
					panic("ASSERT FAILED: %s (%s)", __FUNCTION__, #expr); \
				} \
			} while (0)

#endif

#define	trace()		if (verbosity && dir == DMA_TO_DEVICE) {trace_printk("%s:%d) addr %llx\n", __FUNCTION__, __LINE__, addr);}
#define	trace_debug(...)	if (verbosity) trace_printk(__VA_ARGS__)

#ifndef page_to_virt
#define pfn_to_virt(pfn)	__va((pfn) << PAGE_SHIFT)
#define page_to_virt(page)	pfn_to_virt(page_to_pfn(page))
#endif

#define DMA_ATTRS struct dma_attrs *

static int verbosity;
static inline void set_copy_ops(struct dma_map_ops *target);

static inline void *dma_copy_sync(void *dst, void *src, size_t size)
{
	//trace_debug("syncing %p -> %p [size : %zu]\n", src, dst, size);
	return memcpy(dst, src, size);
}

static inline struct shadow_entry *get_shadow_entry(struct device *dev, dma_addr_t iova)
{
	struct compound_shadow *compound_entry;
	struct copy_mdata *mdata = dev->copy;

	u64 key = dma_cache_iova_key(iova);
	u64 idx = dma_cache_iova_idx(iova);
	u64 e_idx;

	assert(mdata);
	assert(key < (COPY_HASHES));
	assert(idx < MAX_COMPOUND_SHADOW_PER_NODE);
	assert(mdata->compound_entry[key]);
	compound_entry = mdata->compound_entry[key][idx];
	if (unlikely(!compound_entry))
	{
		struct page *page = alloc_pages(__GFP_ZERO, get_order(sizeof(struct compound_shadow)));
		assert(page);
		mdata->compound_entry[key][idx] = page_to_virt(page);
		compound_entry = page_to_virt(page);
	}
	e_idx = ((iova & (BIT(DMA_CACHE_SHIFT) -1))/ MIN_COPY_ALLOC_SZ);
	assert(e_idx < 512);

	//if (verbosity)
	//	iova_format(iova);
	//trace_debug("key %llx idx %llx e_idx %llx <%p>\n", key, idx, e_idx, compound_entry);
	return &compound_entry->entry[e_idx];
}

int dma_copy_register_dev(struct device *dev)
{
	int i = 0;
	struct dma_map_ops *dma_ops = get_dma_ops(dev);
	struct page *page = alloc_pages(__GFP_ZERO,
                                        get_order(sizeof(struct copy_mdata)));
	if (unlikely(!page))
		return -ENOMEM;

	trace_debug("Allocated order %d of pages [%p] orig ops %p\n", get_order(sizeof(struct copy_mdata)), page, dma_ops);
	dev->copy = page_address(page);

	for (; i < COPY_HASHES; i++) {
		 struct page *page = alloc_pages(__GFP_ZERO,
						 get_order(sizeof(struct compound_shadow *) * MAX_COMPOUND_SHADOW_PER_NODE));
		trace_debug("%d: Allocating order %d  [%p]pages\n", i, get_order(sizeof(struct compound_shadow *) * MAX_COMPOUND_SHADOW_PER_NODE), page);
		dev->copy->compound_entry[i] = page_to_virt(page);
		assert(page);
	}

	trace_debug("regitsrer iova map for dev %p\n", dev);
	register_iova_map(dev);
	dev->copy->orig_dma_ops = dma_ops;

	page = alloc_pages(__GFP_ZERO, 0);
	dev->copy->dma_ops = page_address(page);
	memcpy(dev->copy->dma_ops, dma_ops, sizeof(struct dma_map_ops));
	set_copy_ops(dev->copy->dma_ops);

	trace_debug("Set dma_ops orig %p copy %p\n", dev->copy->orig_dma_ops, dev->copy->dma_ops);
	return 0;
}
EXPORT_SYMBOL(dma_copy_register_dev);

//TODO: Need to parse on RX
static dma_addr_t dma_copy_map_page(struct device *dev, struct page *page,
				    unsigned long offset, size_t size,
				    enum dma_data_direction dir,
				    DMA_ATTRS attrs, dma_addr_t unused)
{
	struct shadow_entry *entry;
	u64 addr;
	void *shadow = NULL;

	//TODO: Check size and use real copy on big buffers.
	size = __ALIGN_MASK(size, MIN_COPY_ALLOC_MASK);
	shadow = dma_cache_alloc(dev, size, dir);
	if (!shadow) {
		panic("failed to dma_cache_alloc %s\n", __FUNCTION__);
	}

	addr = virt_to_iova(shadow);
	assert(addr != IOVA_INVALID);

	entry = get_shadow_entry(dev, addr);
	entry->shadow = shadow;
	entry->real = page_address(page) + offset;
	trace_debug("addr %llx [%d] %zd :: %p -> %p \n", addr, dir, size, entry->real, entry->shadow);

	if (dir == DMA_BIDIRECTIONAL || dir == DMA_TO_DEVICE) {
		//real sync for cpu
		dma_copy_sync(shadow, entry->real, size);
		//real sync for device
	}
	//else {
	//	get_page(page);
	//}
	assert(dir == iova_perm(addr));
	//trace();
	return addr;
}

static void dma_copy_unmap(struct device *dev, dma_addr_t addr,
			   size_t size, enum dma_data_direction dir,
			   DMA_ATTRS attrs)
{
	struct shadow_entry *entry = get_shadow_entry(dev, addr);
	struct page *shadow = virt_to_page(entry->shadow);

	assert(dir == iova_perm(addr));

	trace_debug("addr %llx [%d] %zd :: %p -> %p \n", addr, dir, size, entry->shadow, entry->real);
	if (dir == DMA_BIDIRECTIONAL || dir == DMA_FROM_DEVICE) {
		//real sync for cpu
		//TODO: [2] get filterred packet size
		dma_copy_sync(entry->real, entry->shadow, size);
		//put_page(virt_to_page(entry->real));

		//real sync for device
	}

	memset(entry, 0, sizeof(*entry));
	put_page(shadow);
}

static int dma_copy_map_sg(struct device *dev, struct scatterlist *sgl,
			   int nents, enum dma_data_direction dir,
			   DMA_ATTRS attrs)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgl, sg, nents, i) {
		void *va;

		BUG_ON(!sg_page(sg));
		va = sg_virt(sg);
		sg_dma_address(sg) = dma_copy_map_page(dev, sg_page(sg), sg->offset, sg->length, dir, attrs, IOVA_INVALID);
		sg_dma_len(sg) = sg->length;
	}

	return nents;
}

static void dma_copy_unmap_sg(struct device *dev, struct scatterlist *sgl,
			     int nents, enum dma_data_direction dir,
			     DMA_ATTRS attrs)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgl, sg, nents, i) {
		dma_copy_unmap(dev, sg_dma_address(sg), sg_dma_len(sg), dir, attrs);
	}
}

static void dma_copy_sync_single_range_for_cpu(struct device *dev, dma_addr_t addr,
					 unsigned long offset, size_t size,
					 enum dma_data_direction dir)
{
	struct shadow_entry *entry = get_shadow_entry(dev, addr);
	assert(dir == iova_perm(addr));
	//trace();

	if (dir == DMA_BIDIRECTIONAL || dir == DMA_FROM_DEVICE) {
		entry = get_shadow_entry(dev, addr);
		assert(entry->shadow);
		assert(entry->real);
		//real sync for cpu
		dma_copy_sync(entry->real + offset, entry->shadow + offset, size);
		//real sync for device
	} else {
		trace_printk("sync for cpu with %d\n", dir);
	}
}

static void dma_copy_sync_single_range_for_device(struct device *dev, dma_addr_t addr,
						  unsigned long offset, size_t size,
						  enum dma_data_direction dir)
{
	struct shadow_entry *entry = get_shadow_entry(dev, addr);
	assert(dir == iova_perm(addr));
	//trace();

	if (dir == DMA_BIDIRECTIONAL || dir == DMA_FROM_DEVICE) {
		entry = get_shadow_entry(dev, addr);
		assert(entry->shadow);
		assert(entry->real);
		//real sync for cpu
		dma_copy_sync(entry->shadow + offset, entry->real + offset, size);
		//real sync for device
	} else {
		trace_printk("sync for cpu with %d\n", dir);
	}
}

static int dma_copy_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return 0;
}

static int dma_copy_supported(struct device *dev, u64 mask)
{
	return 1;
}

static inline void set_copy_ops(struct dma_map_ops *target)
{
	target->map_page		= dma_copy_map_page;
	target->unmap_page		= dma_copy_unmap;
	target->map_sg			= dma_copy_map_sg;
	target->unmap_sg		= dma_copy_unmap_sg;
	target->mapping_error		= dma_copy_mapping_error;
	target->dma_supported		= dma_copy_supported;
	target->sync_single_range_for_cpu	= dma_copy_sync_single_range_for_cpu;
	target->sync_single_range_for_device	= dma_copy_sync_single_range_for_device;
	//target->sync_sg_for_cpu
	//target->sync_sg_for_device
}

