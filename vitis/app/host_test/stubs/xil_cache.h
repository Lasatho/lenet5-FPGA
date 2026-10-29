// host-test stub: no caches on the host
#ifndef XIL_CACHE_H_
#define XIL_CACHE_H_
#define Xil_DCacheFlushRange(addr, len) ((void)(addr), (void)(len))
#endif
