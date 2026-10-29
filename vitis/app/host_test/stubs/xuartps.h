// host-test stub: UART1 is a pseudo terminal opened by emu.cpp
#ifndef XUARTPS_H_
#define XUARTPS_H_
#include "xil_types.h"
#define XUARTPS_FIFO_OFFSET 0x30
typedef struct { u32 BaseAddress; } XUartPs_Config;
typedef struct { int dummy; } XUartPs;
#ifdef __cplusplus
extern "C" {
#endif
XUartPs_Config *XUartPs_LookupConfig(u32 base);
int XUartPs_CfgInitialize(XUartPs *uart, XUartPs_Config *cfg, u32 base);
int XUartPs_SetBaudRate(XUartPs *uart, u32 baud);
void XUartPs_SendByte(u32 base, u8 data);
u8 XUartPs_RecvByte(u32 base);
int XUartPs_IsReceiveData(u32 base);
u32 XUartPs_ReadReg(u32 base, u32 offset);
#ifdef __cplusplus
}
#endif
#endif
