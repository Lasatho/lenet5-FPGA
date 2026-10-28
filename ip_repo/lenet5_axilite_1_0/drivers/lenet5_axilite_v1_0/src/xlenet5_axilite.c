// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xlenet5_axilite.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XLenet5_axilite_CfgInitialize(XLenet5_axilite *InstancePtr, XLenet5_axilite_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XLenet5_axilite_Start(XLenet5_axilite *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL) & 0x80;
    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XLenet5_axilite_IsDone(XLenet5_axilite *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XLenet5_axilite_IsIdle(XLenet5_axilite *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XLenet5_axilite_IsReady(XLenet5_axilite *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XLenet5_axilite_EnableAutoRestart(XLenet5_axilite *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL, 0x80);
}

void XLenet5_axilite_DisableAutoRestart(XLenet5_axilite *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_AP_CTRL, 0);
}

u32 XLenet5_axilite_Get_logits_BaseAddress(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return (InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE);
}

u32 XLenet5_axilite_Get_logits_HighAddress(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return (InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH);
}

u32 XLenet5_axilite_Get_logits_TotalBytes(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return (XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH - XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 1);
}

u32 XLenet5_axilite_Get_logits_BitWidth(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLENET5_AXILITE_CTRL_WIDTH_LOGITS;
}

u32 XLenet5_axilite_Get_logits_Depth(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLENET5_AXILITE_CTRL_DEPTH_LOGITS;
}

u32 XLenet5_axilite_Write_logits_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length)*4 > (XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH - XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(int *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + (offset + i)*4) = *(data + i);
    }
    return length;
}

u32 XLenet5_axilite_Read_logits_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length)*4 > (XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH - XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(data + i) = *(int *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + (offset + i)*4);
    }
    return length;
}

u32 XLenet5_axilite_Write_logits_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length) > (XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH - XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(char *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + offset + i) = *(data + i);
    }
    return length;
}

u32 XLenet5_axilite_Read_logits_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length) > (XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH - XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(data + i) = *(char *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + offset + i);
    }
    return length;
}

u32 XLenet5_axilite_Get_img_BaseAddress(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return (InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_IMG_BASE);
}

u32 XLenet5_axilite_Get_img_HighAddress(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return (InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH);
}

u32 XLenet5_axilite_Get_img_TotalBytes(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return (XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH - XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + 1);
}

u32 XLenet5_axilite_Get_img_BitWidth(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLENET5_AXILITE_CTRL_WIDTH_IMG;
}

u32 XLenet5_axilite_Get_img_Depth(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLENET5_AXILITE_CTRL_DEPTH_IMG;
}

u32 XLenet5_axilite_Write_img_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length)*4 > (XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH - XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(int *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + (offset + i)*4) = *(data + i);
    }
    return length;
}

u32 XLenet5_axilite_Read_img_Words(XLenet5_axilite *InstancePtr, int offset, word_type *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length)*4 > (XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH - XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(data + i) = *(int *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + (offset + i)*4);
    }
    return length;
}

u32 XLenet5_axilite_Write_img_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length) > (XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH - XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(char *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + offset + i) = *(data + i);
    }
    return length;
}

u32 XLenet5_axilite_Read_img_Bytes(XLenet5_axilite *InstancePtr, int offset, char *data, int length) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr -> IsReady == XIL_COMPONENT_IS_READY);

    int i;

    if ((offset + length) > (XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH - XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + 1))
        return 0;

    for (i = 0; i < length; i++) {
        *(data + i) = *(char *)(InstancePtr->Ctrl_BaseAddress + XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + offset + i);
    }
    return length;
}

void XLenet5_axilite_InterruptGlobalEnable(XLenet5_axilite *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_GIE, 1);
}

void XLenet5_axilite_InterruptGlobalDisable(XLenet5_axilite *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_GIE, 0);
}

void XLenet5_axilite_InterruptEnable(XLenet5_axilite *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_IER);
    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_IER, Register | Mask);
}

void XLenet5_axilite_InterruptDisable(XLenet5_axilite *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_IER);
    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_IER, Register & (~Mask));
}

void XLenet5_axilite_InterruptClear(XLenet5_axilite *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XLenet5_axilite_WriteReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_ISR, Mask);
}

u32 XLenet5_axilite_InterruptGetEnabled(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_IER);
}

u32 XLenet5_axilite_InterruptGetStatus(XLenet5_axilite *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XLenet5_axilite_ReadReg(InstancePtr->Ctrl_BaseAddress, XLENET5_AXILITE_CTRL_ADDR_ISR);
}

