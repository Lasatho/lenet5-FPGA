// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2024.2 (64-bit)
// Tool Version Limit: 2024.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// ctrl
// 0x000 : Control signals
//         bit 0  - ap_start (Read/Write/COH)
//         bit 1  - ap_done (Read/COR)
//         bit 2  - ap_idle (Read)
//         bit 3  - ap_ready (Read/COR)
//         bit 7  - auto_restart (Read/Write)
//         bit 9  - interrupt (Read)
//         others - reserved
// 0x004 : Global Interrupt Enable Register
//         bit 0  - Global Interrupt Enable (Read/Write)
//         others - reserved
// 0x008 : IP Interrupt Enable Register (Read/Write)
//         bit 0 - enable ap_done interrupt (Read/Write)
//         bit 1 - enable ap_ready interrupt (Read/Write)
//         others - reserved
// 0x00c : IP Interrupt Status Register (Read/TOW)
//         bit 0 - ap_done (Read/TOW)
//         bit 1 - ap_ready (Read/TOW)
//         others - reserved
// 0x040 ~
// 0x07f : Memory 'logits' (10 * 32b)
//         Word n : bit [31:0] - logits[n]
// 0x800 ~
// 0xfff : Memory 'img' (1024 * 16b)
//         Word n : bit [15: 0] - img[2n]
//                  bit [31:16] - img[2n+1]
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XLENET5_AXILITE_CTRL_ADDR_AP_CTRL     0x000
#define XLENET5_AXILITE_CTRL_ADDR_GIE         0x004
#define XLENET5_AXILITE_CTRL_ADDR_IER         0x008
#define XLENET5_AXILITE_CTRL_ADDR_ISR         0x00c
#define XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE 0x040
#define XLENET5_AXILITE_CTRL_ADDR_LOGITS_HIGH 0x07f
#define XLENET5_AXILITE_CTRL_WIDTH_LOGITS     32
#define XLENET5_AXILITE_CTRL_DEPTH_LOGITS     10
#define XLENET5_AXILITE_CTRL_ADDR_IMG_BASE    0x800
#define XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH    0xfff
#define XLENET5_AXILITE_CTRL_WIDTH_IMG        16
#define XLENET5_AXILITE_CTRL_DEPTH_IMG        1024

