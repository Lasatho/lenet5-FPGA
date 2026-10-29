// host-test stub: only what main.c uses from the HLS driver
#ifndef XLENET5_AXILITE_H_
#define XLENET5_AXILITE_H_
#include "xil_types.h"
typedef struct { int dummy; } XLenet5_axilite;
#ifdef __cplusplus
extern "C" {
#endif
int XLenet5_axilite_Initialize(XLenet5_axilite *ip, UINTPTR base);
#ifdef __cplusplus
}
#endif
#endif
