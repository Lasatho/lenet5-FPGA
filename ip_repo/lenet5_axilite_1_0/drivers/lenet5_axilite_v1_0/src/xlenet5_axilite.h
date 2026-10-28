// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XLENET5_AXILITE_H
#define XLENET5_AXILITE_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xlenet5_axilite_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
#ifdef SDT
    char *Name;
#else
    u16 DeviceId;
#endif
    u64 Ctrl_BaseAddress;
} XLenet5_axilite_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XLenet5_axilite;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XLenet5_axilite_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XLenet5_axilite_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XLenet5_axilite_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XLenet5_axilite_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
#ifdef SDT
int XLenet5_axilite_Initialize(XLenet5_axilite *InstancePtr, UINTPTR BaseAddress);
XLenet5_axilite_Config* XLenet5_axilite_LookupConfig(UINTPTR BaseAddress);
#else
int XLenet5_axilite_Initialize(XLenet5_axilite *InstancePtr, u16 DeviceId);
XLenet5_axilite_Config* XLenet5_axilite_LookupConfig(u16 DeviceId);
#endif
int XLenet5_axilite_CfgInitialize(XLenet5_axilite *InstancePtr, XLenet5_axilite_Config *ConfigPtr);
#else
int XLenet5_axilite_Initialize(XLenet5_axilite *InstancePtr, const char* InstanceName);
int XLenet5_axilite_Release(XLenet5_axilite *InstancePtr);
#endif

void XLenet5_axilite_Start(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_IsDone(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_IsIdle(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_IsReady(XLenet5_axilite *InstancePtr);
void XLenet5_axilite_EnableAutoRestart(XLenet5_axilite *InstancePtr);
void XLenet5_axilite_DisableAutoRestart(XLenet5_axilite *InstancePtr);

u32 XLenet5_axilite_Get_logits_BaseAddress(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_logits_HighAddress(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_logits_TotalBytes(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_logits_BitWidth(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_logits_Depth(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Write_logits_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length);
u32 XLenet5_axilite_Read_logits_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length);
u32 XLenet5_axilite_Write_logits_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length);
u32 XLenet5_axilite_Read_logits_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length);
u32 XLenet5_axilite_Get_img_BaseAddress(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_img_HighAddress(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_img_TotalBytes(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_img_BitWidth(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Get_img_Depth(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_Write_img_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length);
u32 XLenet5_axilite_Read_img_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length);
u32 XLenet5_axilite_Write_img_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length);
u32 XLenet5_axilite_Read_img_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length);

void XLenet5_axilite_InterruptGlobalEnable(XLenet5_axilite *InstancePtr);
void XLenet5_axilite_InterruptGlobalDisable(XLenet5_axilite *InstancePtr);
void XLenet5_axilite_InterruptEnable(XLenet5_axilite *InstancePtr, u32 Mask);
void XLenet5_axilite_InterruptDisable(XLenet5_axilite *InstancePtr, u32 Mask);
void XLenet5_axilite_InterruptClear(XLenet5_axilite *InstancePtr, u32 Mask);
u32 XLenet5_axilite_InterruptGetEnabled(XLenet5_axilite *InstancePtr);
u32 XLenet5_axilite_InterruptGetStatus(XLenet5_axilite *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
