// Host emulation of the board for vitis/app/src/main.c (no Vitis, no board): the stub
// headers in stubs/ redirect the BSP calls here.
// - lenet5_axilite: register file as in xlenet5_axilite_hw.h, ap_start runs the C model
//   myproject() with the image words fed in like the wrapper's read_image()
// - UART1: pseudo terminal, its path is written to $EMU_PTY_FILE
// - DDR window 0x10000000..0x11ffffff: shared mapping of $EMU_DDR_FILE, so fake_xsct.py
//   can load the test set and read the results like jtag.tcl does over JTAG
// Build and run: see run_host_test.sh.

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

#include "myproject.h"

extern "C" {
#include "xil_io.h"
#include "xiltimer.h"
#include "xlenet5_axilite.h"
#include "xlenet5_axilite_hw.h"
#include "xparameters.h"
#include "xuartps.h"
}

static const UINTPTR DDR_BASE = 0x10000000;
static const size_t DDR_SIZE = 0x02000000;

typedef input_t::value_type pixel_t;
typedef result_t::value_type logit_t;

static int pty = -1;
static u32 img_words[512];
static u32 logit_words[10];
static bool done = false;

__attribute__((constructor)) static void emu_init() {
    pty = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty < 0 || grantpt(pty) || unlockpt(pty)) {
        perror("pty");
        std::exit(1);
    }
    struct termios t;
    tcgetattr(pty, &t);
    cfmakeraw(&t);
    tcsetattr(pty, TCSANOW, &t);
    FILE *f = std::fopen(std::getenv("EMU_PTY_FILE"), "w");
    std::fprintf(f, "%s\n", ptsname(pty));
    std::fclose(f);

    int fd = open(std::getenv("EMU_DDR_FILE"), O_RDWR | O_CREAT, 0644);
    if (fd < 0 || ftruncate(fd, DDR_SIZE) ||
        mmap((void *)DDR_BASE, DDR_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED_NOREPLACE, fd, 0) ==
            MAP_FAILED) {
        perror("ddr");
        std::exit(1);
    }
    // myproject() loads its weights from weights/*.txt relative to the working directory
    if (chdir(std::getenv("EMU_FIRMWARE_DIR"))) {
        perror("chdir");
        std::exit(1);
    }
}

static void run_ip() {
    hls::stream<input_t> x("x");
    hls::stream<result_t> y("y");
    for (int i = 0; i < 1024; i++) {
        ap_int<16> word = (i % 2) ? (img_words[i / 2] >> 16) : (img_words[i / 2] & 0xFFFF);
        input_t pack;
        pack[0].range() = word.range(pixel_t::width - 1, 0);
        x.write(pack);
    }
    myproject(x, y);
    result_t out = y.read();
    for (int j = 0; j < 10; j++) {
        ap_int<logit_t::width> raw;
        raw.range() = out[j].range();
        logit_words[j] = (u32)raw.to_int();
    }
    done = true;
}

extern "C" {

void Xil_Out32(UINTPTR addr, u32 value) {
    UINTPTR off = addr - XPAR_XLENET5_AXILITE_0_BASEADDR;
    if (off >= XLENET5_AXILITE_CTRL_ADDR_IMG_BASE && off <= XLENET5_AXILITE_CTRL_ADDR_IMG_HIGH)
        img_words[(off - XLENET5_AXILITE_CTRL_ADDR_IMG_BASE) / 4] = value;
    else if (off == XLENET5_AXILITE_CTRL_ADDR_AP_CTRL && (value & 1))
        run_ip();
}

u32 Xil_In32(UINTPTR addr) {
    if (addr == XPAR_GLOBAL_TMR_BASEADDR + 0x08)
        return 1;   // global timer already running
    UINTPTR off = addr - XPAR_XLENET5_AXILITE_0_BASEADDR;
    if (off == XLENET5_AXILITE_CTRL_ADDR_AP_CTRL) {
        u32 v = (done ? 0x2 : 0) | 0x4;   // ap_done is clear-on-read
        done = false;
        return v;
    }
    if (off >= XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE && off < XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE + 40)
        return logit_words[(off - XLENET5_AXILITE_CTRL_ADDR_LOGITS_BASE) / 4];
    return 0;
}

void XTime_GetTime(XTime *t) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *t = ((u64)ts.tv_sec * 1000000000u + ts.tv_nsec) * 325 / 1000;
}

int XLenet5_axilite_Initialize(XLenet5_axilite *, UINTPTR) { return 0; }

static XUartPs_Config uart_cfg = {XPAR_UART1_BASEADDR};
XUartPs_Config *XUartPs_LookupConfig(u32) { return &uart_cfg; }
int XUartPs_CfgInitialize(XUartPs *, XUartPs_Config *, u32) { return 0; }
int XUartPs_SetBaudRate(XUartPs *, u32) { return 0; }

void XUartPs_SendByte(u32, u8 data) {
    if (write(pty, &data, 1) != 1)
        std::exit(0);
}

u8 XUartPs_RecvByte(u32) {
    u8 b;
    while (read(pty, &b, 1) != 1)
        usleep(1000);   // EIO until the client opens the pty
    return b;
}

int XUartPs_IsReceiveData(u32) {
    struct pollfd p = {pty, POLLIN, 0};
    return poll(&p, 1, 0) > 0 && (p.revents & POLLIN);
}

u32 XUartPs_ReadReg(u32, u32) { return XUartPs_RecvByte(0); }

}
