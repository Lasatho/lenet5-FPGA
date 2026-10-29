// host-test stub: global timer from clock_gettime, nominal 325 MHz ticks
#ifndef XILTIMER_H_
#define XILTIMER_H_
#include "xil_types.h"
typedef u64 XTime;
#define COUNTS_PER_SECOND 325000000u
#ifdef __cplusplus
extern "C" {
#endif
void XTime_GetTime(XTime *t);
#ifdef __cplusplus
}
#endif
#endif
