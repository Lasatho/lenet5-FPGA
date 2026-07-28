#ifndef MYPROJECT_H_
#define MYPROJECT_H_

#include "ap_fixed.h"
#include "ap_int.h"
#include "hls_stream.h"

#include "defines.h"

// Prototype of top level function for C-synthesis
void myproject(
    hls::stream<input_t> &input_1,
    hls::stream<result_t> &layer14_out,
    conv2d_weight_t w2[150], 
    conv2d_1_weight_t w5[2400], 
    dense_weight_t w9[48000], 
    dense_bias_t b9[120], 
    dense_1_weight_t w11[10080], 
    dense_2_weight_t w13[840]
);

#endif
