// Bare-metal app for lenet5_axilite on the ZYBO: receives images over UART1 (or reads
// them from DDR, loaded via JTAG), runs the IP and returns logits and timings.
// Protocol and DDR layout in protocol.h.

#include "xil_cache.h"
#include "xil_io.h"
#include "xil_types.h"
#include "xiltimer.h"
#include "xlenet5_axilite.h"
#include "xlenet5_axilite_hw.h"
#include "xparameters.h"
#include "xuartps.h"

#include "protocol.h"

#ifndef UART_BAUD
#define UART_BAUD 921600
#endif

#define UART_BASE          XPAR_UART1_BASEADDR
#define IP_BASE            XPAR_XLENET5_AXILITE_0_BASEADDR
#define FRAME_TIMEOUT_US   50000
#define IP_TIMEOUT_US      100000

#define AP_START           0x1
#define AP_DONE            0x2
#define AP_IDLE            0x4

// global timer control register (Cortex-A9 MPCore), bit 0 = enable
#define GTIMER_CONTROL     (XPAR_GLOBAL_TMR_BASEADDR + 0x08)

static XUartPs uart;
static XLenet5_axilite ip;
static u8 payload[MAX_PAYLOAD] __attribute__((aligned(4)));

static u64 us_to_ticks(u32 us) {
    return (u64)us * COUNTS_PER_SECOND / 1000000;
}

static u16 crc16_update(u16 crc, const u8 *data, u32 len) {
    for (u32 i = 0; i < len; i++) {
        crc ^= (u16)data[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

// ---------------------------------------------------------------- UART

static void send_bytes(const u8 *data, u32 len) {
    for (u32 i = 0; i < len; i++)
        XUartPs_SendByte(UART_BASE, data[i]);
}

static u8 recv_byte(void) {
    return XUartPs_RecvByte(UART_BASE);
}

// returns 0 on timeout
static int recv_byte_timeout(u8 *b, u64 timeout) {
    XTime start, now;
    XTime_GetTime(&start);
    while (!XUartPs_IsReceiveData(UART_BASE)) {
        XTime_GetTime(&now);
        if (now - start > timeout)
            return 0;
    }
    *b = XUartPs_ReadReg(UART_BASE, XUARTPS_FIFO_OFFSET);
    return 1;
}

static void send_response(u8 cmd, u8 status, const void *data, u16 len) {
    u8 head[5] = {SYNC_RESPONSE, cmd, status, (u8)len, (u8)(len >> 8)};
    u16 crc = crc16_update(0xFFFF, head + 1, 4);
    crc = crc16_update(crc, (const u8 *)data, len);
    u8 tail[2] = {(u8)crc, (u8)(crc >> 8)};
    send_bytes(head, 5);
    send_bytes((const u8 *)data, len);
    send_bytes(tail, 2);
}

// Waits for the sync byte, then reads one request with a timeout between bytes.
// Returns STATUS_OK and fills cmd/len/payload, or an error status.
static int recv_request(u8 *cmd, u16 *len) {
    u64 timeout = us_to_ticks(FRAME_TIMEOUT_US);
    u8 head[3], tail[2];

    while (recv_byte() != SYNC_REQUEST)
        ;
    for (int i = 0; i < 3; i++)
        if (!recv_byte_timeout(&head[i], timeout))
            return STATUS_FRAME;
    *cmd = head[0];
    *len = head[1] | (head[2] << 8);
    if (*len > MAX_PAYLOAD)
        return STATUS_LENGTH;
    for (u32 i = 0; i < *len; i++)
        if (!recv_byte_timeout(&payload[i], timeout))
            return STATUS_FRAME;
    for (int i = 0; i < 2; i++)
        if (!recv_byte_timeout(&tail[i], timeout))
            return STATUS_FRAME;

    u16 crc = crc16_update(crc16_update(0xFFFF, head, 3), payload, *len);
    if (crc != (u16)(tail[0] | (tail[1] << 8)))
        return STATUS_CRC;
    return STATUS_OK;
}

// ---------------------------------------------------------------- IP

// Runs one image (512 packed words, img[2n] in bits 15..0, img[2n+1] in 31..16, which is
// the little-endian int16 array reinterpreted as u32). Returns STATUS_OK or
// STATUS_IP_TIMEOUT.
static int run_image(const u32 *words, s32 logits[N_CLASSES], u32 *t_core, u32 *t_total) {
    XTime t0, t1, t2, t3, now;
    u64 timeout = us_to_ticks(IP_TIMEOUT_US);

    XTime_GetTime(&t0);
    for (int i = 0; i < N_PIXELS / 2; i++)
        Xil_Out32(IP_BASE + XLENET5_AXILITE_CTRL_ADDR_IMG_BASE + 4 * i, words[i]);

    XTime_GetTime(&t1);
    Xil_Out32(IP_BASE + XLENET5_AXILITE_CTRL_ADDR_AP_CTRL, AP_START);
    // ap_done is clear-on-read: one read that sees it ends the wait
    while (!(Xil_In32(IP_BASE + XLENET5_AXILITE_CTRL_ADDR_AP_CTRL) & AP_DONE)) {
        XTime_GetTime(&now);
        if (now - t1 > timeout)
            return STATUS_IP_TIMEOUT;
    }
    XTime_GetTime(&t2);

    for (int j = 0; j < N_CLASSES; j++)
        logits[j] = (s32)Xil_In32(IP_BASE + XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 4 * j);
    XTime_GetTime(&t3);

    *t_core = (u32)(t2 - t1);
    *t_total = (u32)(t3 - t0);
    return STATUS_OK;
}

static u8 argmax(const s32 logits[N_CLASSES]) {
    u8 best = 0;
    for (u8 j = 1; j < N_CLASSES; j++)
        if (logits[j] > logits[best])
            best = j;
    return best;
}

// ---------------------------------------------------------------- commands

static void cmd_ping(void) {
    PingReply r = {PROTO_VERSION, COUNTS_PER_SECOND, UART_BAUD};
    send_response(CMD_PING, STATUS_OK, &r, sizeof(r));
}

static void cmd_infer(u16 len) {
    InferReply r;
    if (len != N_PIXELS * 2) {
        send_response(CMD_INFER, STATUS_LENGTH, 0, 0);
        return;
    }
    s32 logits[N_CLASSES];
    u32 t_core, t_total;
    int status = run_image((const u32 *)payload, logits, &t_core, &t_total);
    if (status != STATUS_OK) {
        send_response(CMD_INFER, status, 0, 0);
        return;
    }
    // InferReply is packed, t_core/t_total are unaligned: copy instead of passing pointers
    for (int j = 0; j < N_CLASSES; j++)
        r.logits[j] = logits[j];
    r.argmax = argmax(logits);
    r.t_core = t_core;
    r.t_total = t_total;
    send_response(CMD_INFER, STATUS_OK, &r, sizeof(r));
}

static void cmd_ddr(u16 len) {
    DdrReply r = {0};
    u32 n;
    if (len != 4 || (n = *(const u32 *)payload) == 0 || n > DDR_MAX_IMAGES) {
        send_response(CMD_DDR, STATUS_LENGTH, 0, 0);
        return;
    }
    const u32 *images = (const u32 *)(UINTPTR)DDR_IMAGE_BASE;
    const u8 *labels = (const u8 *)(UINTPTR)DDR_LABEL_BASE;
    DdrResult *results = (DdrResult *)(UINTPTR)DDR_RESULT_BASE;

    // images and labels were written by JTAG; flush (clean + invalidate) instead of a
    // plain invalidate, so the data is right whether the debugger wrote past the cache
    // or through it
    Xil_DCacheFlushRange(DDR_IMAGE_BASE, n * N_PIXELS * 2);
    Xil_DCacheFlushRange(DDR_LABEL_BASE, n);

    r.t_core_min = 0xFFFFFFFF;
    XTime start, end;
    XTime_GetTime(&start);
    for (u32 k = 0; k < n; k++) {
        s32 logits[N_CLASSES];
        u32 t_core, t_total;
        int status = run_image(images + k * (N_PIXELS / 2), logits, &t_core, &t_total);
        if (status != STATUS_OK) {
            r.n = k;
            send_response(CMD_DDR, status, &r, sizeof(r));
            return;
        }
        for (int j = 0; j < N_CLASSES; j++)
            results[k].logits[j] = logits[j];
        results[k].t_core = t_core;
        results[k].t_total = t_total;
        r.correct += argmax(logits) == labels[k];
        r.t_core_sum += t_core;
        r.t_total_sum += t_total;
        if (t_core < r.t_core_min)
            r.t_core_min = t_core;
        if (t_core > r.t_core_max)
            r.t_core_max = t_core;
    }
    XTime_GetTime(&end);
    r.n = n;
    r.t_run = end - start;

    // results are read back by JTAG (xsct mrd), which does not see the data cache
    Xil_DCacheFlushRange(DDR_RESULT_BASE, n * sizeof(DdrResult));
    send_response(CMD_DDR, STATUS_OK, &r, sizeof(r));
}

int main(void) {
    // xiltimer starts the global timer only for sleep(); enable it if it is still off,
    // without resetting the counter
    if (!(Xil_In32(GTIMER_CONTROL) & 0x1))
        Xil_Out32(GTIMER_CONTROL, Xil_In32(GTIMER_CONTROL) | 0x1);

    XUartPs_Config *uart_cfg = XUartPs_LookupConfig(UART_BASE);
    XUartPs_CfgInitialize(&uart, uart_cfg, uart_cfg->BaseAddress);
    XUartPs_SetBaudRate(&uart, UART_BAUD);
    XLenet5_axilite_Initialize(&ip, IP_BASE);

    for (;;) {
        u8 cmd;
        u16 len;
        int status = recv_request(&cmd, &len);
        if (status != STATUS_OK) {
            send_response(0, status, 0, 0);
            continue;
        }
        switch (cmd) {
        case CMD_PING:  cmd_ping(); break;
        case CMD_INFER: cmd_infer(len); break;
        case CMD_DDR:   cmd_ddr(len); break;
        default:        send_response(cmd, STATUS_COMMAND, 0, 0); break;
        }
    }
}
