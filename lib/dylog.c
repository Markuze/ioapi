/***
	Dynamic logging of io, abstraction for dynamic loading/unloading of loggers
****/
#include <linux/kernel.h>
#include <linux/dylog.h>

static struct dylog global_dylog;

void __dylog_trace_alloc(u64 addr, u16 size, int is_tx, int is_alloc)
{
	if (global_dylog.trace_alloc)
		global_dylog.trace_alloc(addr, size, is_tx, is_alloc);
}
EXPORT_SYMBOL(__dylog_trace_alloc);

void dylog_trace_napi(uint32_t tx, uint32_t rx)
{
	if (global_dylog.trace_napi)
		global_dylog.trace_napi(tx, rx);
}
EXPORT_SYMBOL(dylog_trace_napi);

void dylog_register(trace_alloc_ptr trace_alloc, trace_napi_ptr trace_napi)
{
	global_dylog.trace_alloc = trace_alloc;
	global_dylog.trace_napi = trace_napi;
}
EXPORT_SYMBOL(dylog_register);

void dylog_unregister(void)
{
	global_dylog.trace_alloc = NULL;
	global_dylog.trace_napi = NULL;
}
EXPORT_SYMBOL(dylog_unregister);
