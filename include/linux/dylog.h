#ifndef DYLOG__H
#define DYLOG__H

typedef void (*trace_alloc_ptr)(u64 addr, u16 size, int is_tx, int is_alloc);
typedef void (*trace_napi_ptr)(uint32_t tx, uint32_t rx);

struct dylog {
	trace_alloc_ptr trace_alloc;
	trace_napi_ptr trace_napi;
};

#define dylog_trace_alloc(a, s, t) __dylog_trace_alloc((u64)a, s, t, 1);
#define dylog_trace_free(a) __dylog_trace_alloc((u64)a, 0, 0, 0);
void __dylog_trace_alloc(u64 addr, u16 size, int is_tx, int is_alloc);
void dylog_trace_napi(uint32_t tx, uint32_t rx);
void dylog_register(trace_alloc_ptr trace_alloc, trace_napi_ptr trace_napi);
void dylog_unregister(void);

#endif /*DYLOG__H*/
