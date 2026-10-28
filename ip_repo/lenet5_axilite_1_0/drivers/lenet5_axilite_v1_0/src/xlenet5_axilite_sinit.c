// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#ifdef SDT
#include "xparameters.h"
#endif
#include "xlenet5_axilite.h"

extern XLenet5_axilite_Config XLenet5_axilite_ConfigTable[];

#ifdef SDT
XLenet5_axilite_Config *XLenet5_axilite_LookupConfig(UINTPTR BaseAddress) {
	XLenet5_axilite_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XLenet5_axilite_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XLenet5_axilite_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XLenet5_axilite_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XLenet5_axilite_Initialize(XLenet5_axilite *InstancePtr, UINTPTR BaseAddress) {
	XLenet5_axilite_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XLenet5_axilite_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XLenet5_axilite_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XLenet5_axilite_Config *XLenet5_axilite_LookupConfig(u16 DeviceId) {
	XLenet5_axilite_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XLENET5_AXILITE_NUM_INSTANCES; Index++) {
		if (XLenet5_axilite_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XLenet5_axilite_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XLenet5_axilite_Initialize(XLenet5_axilite *InstancePtr, u16 DeviceId) {
	XLenet5_axilite_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XLenet5_axilite_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XLenet5_axilite_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

