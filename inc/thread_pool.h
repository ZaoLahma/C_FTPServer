#ifndef HEADER_THREAD_POOL
#define HEADER_THREAD_POOL

struct ThreadContext;

extern struct ThreadContext* init_thread_pool(unsigned int maxThreads);
extern void sched_job(struct ThreadContext* context, void* (*thread_func)(void*), void* args);
extern void destroy_thread_pool(struct ThreadContext* context);

#endif
