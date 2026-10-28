#ifndef LENET5_AXILITE_H_
#define LENET5_AXILITE_H_

#include "ap_int.h"
#include "myproject.h"

// AXI-Lite wrapper around the hls4ml core myproject() (AXI-Stream without TLAST,
// ap_ctrl_hs). Image, logits and block control are all mapped to one s_axilite slave.

static const int N_PIXELS = 32 * 32;
static const int N_CLASSES = 10;

typedef input_t::value_type pixel_t;   // ap_fixed<11,5>
typedef result_t::value_type logit_t;  // ap_fixed<28,16>

// pixel: raw ap_fixed<11,5> bits, sign-extended to 16 bit
typedef ap_int<16> pixel_word_t;
// logit: raw ap_fixed<28,16> bits, sign-extended to 32 bit; value = word / 2^LOGIT_FRAC_BITS
typedef ap_int<32> logit_word_t;
static const int LOGIT_FRAC_BITS = logit_t::width - logit_t::iwidth;

void lenet5_axilite(const pixel_word_t img[N_PIXELS], logit_word_t logits[N_CLASSES]);

#endif
