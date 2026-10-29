#ifndef PROTOCOL_H_
#define PROTOCOL_H_

// UART protocol between pc_client/send_image.py and the bare-metal app (keep both in sync).
// Binary, little-endian, stop-and-wait: the PC sends one request and waits for the reply.
//
// request  (PC -> board): 0xA5 | cmd u8 | len u16 | payload[len] | crc u16
// response (board -> PC): 0x5A | cmd u8 | status u8 | len u16 | payload[len] | crc u16
//
// crc: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over everything between the sync
// byte and the crc, i.e. Python binascii.crc_hqx(frame[1:-2], 0xFFFF).

#define PROTO_VERSION     1

#define SYNC_REQUEST      0xA5
#define SYNC_RESPONSE     0x5A

// ping: no payload; reply PingReply
#define CMD_PING          'P'
// inference of one image: payload 1024 x int16 (raw ap_fixed<11,5> words, row-major
// 32x32, from pc_client/data/mnist_test_q.bin); reply InferReply
#define CMD_INFER         'I'
// DDR mode: payload u32 number of images; images and labels were loaded into DDR via
// JTAG (vitis/jtag.tcl), per-image results go to DDR_RESULT_BASE; reply DdrReply
#define CMD_DDR           'D'

#define STATUS_OK         0
#define STATUS_CRC        1   // request crc mismatch
#define STATUS_LENGTH     2   // payload length invalid for the command
#define STATUS_COMMAND    3   // unknown command
#define STATUS_IP_TIMEOUT 4   // ap_done not seen within IP_TIMEOUT_US
#define STATUS_FRAME      5   // request incomplete (gap > FRAME_TIMEOUT_US between bytes)

#define N_PIXELS          1024
#define N_CLASSES         10
#define MAX_PAYLOAD       (N_PIXELS * 2)

// DDR mode layout (DDR 0x00100000..0x1fffffff, the app is linked to the low end)
#define DDR_IMAGE_BASE    0x10000000u   // N x 1024 x int16
#define DDR_LABEL_BASE    0x11400000u   // N x uint8
#define DDR_RESULT_BASE   0x11500000u   // N x DdrResult
#define DDR_MAX_IMAGES    10000

#pragma pack(push, 1)
typedef struct {
    u16 proto_version;
    u32 timer_hz;          // tick rate of t_core/t_total (global timer)
    u32 baud;
} PingReply;

typedef struct {
    s32 logits[N_CLASSES]; // raw ap_fixed<28,16> bits, sign-extended; value = word / 2^12
    u8  argmax;
    u32 t_core;            // ticks from ap_start to ap_done (model inference)
    u32 t_total;           // ticks incl. writing the image and reading the logits
} InferReply;

typedef struct {
    s32 logits[N_CLASSES];
    u32 t_core;
    u32 t_total;
} DdrResult;               // 48 bytes per image at DDR_RESULT_BASE

typedef struct {
    u32 n;
    u32 correct;           // argmax == label
    u32 t_core_min;
    u32 t_core_max;
    u64 t_core_sum;
    u64 t_total_sum;
    u64 t_run;             // ticks for the whole loop over n images
} DdrReply;
#pragma pack(pop)

#endif
