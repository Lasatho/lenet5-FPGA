#ifndef DEFINES_H_
#define DEFINES_H_

#include "ap_fixed.h"
#include "ap_int.h"
#include "nnet_utils/nnet_types.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <tuple>
#include <tuple>


// hls-fpga-machine-learning insert numbers

// hls-fpga-machine-learning insert layer-precision
typedef nnet::array<ap_fixed<11,5>, 1*1> input_t;
typedef ap_fixed<28,16> layer1_0_accum_t;
typedef nnet::array<ap_fixed<28,16>, 6*1> layer1_0_result_t;
typedef ap_fixed<11,5> layer1_0_weight_t;
typedef ap_fixed<11,5> layer1_0_bias_t;
typedef nnet::array<ap_fixed<11,5>, 6*1> layer4_t;
typedef ap_fixed<18,8> layer1_2_table_t;
typedef ap_fixed<11,5> layer1_3_accum_t;
typedef nnet::array<ap_fixed<11,5>, 6*1> layer5_t;
typedef ap_fixed<31,19> layer2_0_accum_t;
typedef nnet::array<ap_fixed<31,19>, 16*1> layer2_0_result_t;
typedef ap_fixed<11,5> layer2_0_weight_t;
typedef ap_fixed<11,5> layer2_0_bias_t;
typedef nnet::array<ap_fixed<11,5>, 16*1> layer8_t;
typedef ap_fixed<18,8> layer2_2_table_t;
typedef ap_fixed<11,5> layer2_3_accum_t;
typedef nnet::array<ap_fixed<11,5>, 16*1> layer9_t;
typedef ap_fixed<32,20> fc_accum_t;
typedef nnet::array<ap_fixed<11,5,AP_TRN,AP_SAT,0>, 36*1> layer11_t;
typedef ap_fixed<11,5> fc_weight_t;
typedef ap_fixed<11,5> fc_bias_t;
typedef ap_uint<1> layer11_index;
typedef nnet::array<ap_fixed<11,5>, 36*1> layer12_t;
typedef ap_fixed<18,8> relu_table_t;
typedef ap_fixed<29,17> fc1_accum_t;
typedef nnet::array<ap_fixed<11,5,AP_TRN,AP_SAT,0>, 25*1> layer13_t;
typedef ap_fixed<11,5> fc1_weight_t;
typedef ap_fixed<11,5> fc1_bias_t;
typedef ap_uint<1> layer13_index;
typedef nnet::array<ap_fixed<11,5>, 25*1> layer14_t;
typedef ap_fixed<18,8> relu1_table_t;
typedef ap_fixed<28,16> fc2_accum_t;
typedef nnet::array<ap_fixed<28,16>, 10*1> result_t;
typedef ap_fixed<11,5> fc2_weight_t;
typedef ap_fixed<11,5> fc2_bias_t;
typedef ap_uint<1> layer15_index;

// hls-fpga-machine-learning insert emulator-defines


#endif
