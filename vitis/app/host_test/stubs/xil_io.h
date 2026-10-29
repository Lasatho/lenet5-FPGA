// host-test stub: register access goes to the emulated IP in emu.cpp
#ifndef XIL_IO_H_
#define XIL_IO_H_
#include "xil_types.h"
#ifdef __cplusplus
extern "C" {
#endif
void Xil_Out32(UINTPTR addr, u32 value);
u32 Xil_In32(UINTPTR addr);
#ifdef __cplusplus
}
#endif
#endif
