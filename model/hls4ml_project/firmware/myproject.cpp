#include <iostream>

#include "myproject.h"
#include "parameters.h"


void myproject(
    input_t input_layer[32*32*1],
    result_t layer14_out[10]
) {

    // hls-fpga-machine-learning insert IO
    #pragma HLS ARRAY_RESHAPE variable=input_layer complete dim=0
    #pragma HLS ARRAY_PARTITION variable=layer14_out complete dim=0
    #pragma HLS INTERFACE ap_vld port=input_layer,layer14_out 
    #pragma HLS DATAFLOW

    // hls-fpga-machine-learning insert load weights
#ifndef __SYNTHESIS__
    static bool loaded_weights = false;
    if (!loaded_weights) {
        nnet::load_weights_from_txt<model_default_t, 150>(w2, "w2.txt");
        nnet::load_weights_from_txt<model_default_t, 6>(b2, "b2.txt");
        nnet::load_weights_from_txt<model_default_t, 2400>(w5, "w5.txt");
        nnet::load_weights_from_txt<model_default_t, 16>(b5, "b5.txt");
        nnet::load_weights_from_txt<model_default_t, 48000>(w9, "w9.txt");
        nnet::load_weights_from_txt<model_default_t, 120>(b9, "b9.txt");
        nnet::load_weights_from_txt<model_default_t, 10080>(w11, "w11.txt");
        nnet::load_weights_from_txt<model_default_t, 84>(b11, "b11.txt");
        nnet::load_weights_from_txt<model_default_t, 840>(w13, "w13.txt");
        nnet::load_weights_from_txt<model_default_t, 10>(b13, "b13.txt");
        loaded_weights = true;    }
#endif
    // ****************************************
    // NETWORK INSTANTIATION
    // ****************************************

    // hls-fpga-machine-learning insert layers

    layer2_t layer2_out[28*28*6];
    #pragma HLS ARRAY_PARTITION variable=layer2_out complete dim=0

    layer3_t layer3_out[28*28*6];
    #pragma HLS ARRAY_PARTITION variable=layer3_out complete dim=0

    layer4_t layer4_out[14*14*6];
    #pragma HLS ARRAY_PARTITION variable=layer4_out complete dim=0

    layer5_t layer5_out[10*10*16];
    #pragma HLS ARRAY_PARTITION variable=layer5_out complete dim=0

    layer6_t layer6_out[10*10*16];
    #pragma HLS ARRAY_PARTITION variable=layer6_out complete dim=0

    layer7_t layer7_out[5*5*16];
    #pragma HLS ARRAY_PARTITION variable=layer7_out complete dim=0

    auto& layer8_out = layer7_out;
    layer9_t layer9_out[120];
    #pragma HLS ARRAY_PARTITION variable=layer9_out complete dim=0

    layer10_t layer10_out[120];
    #pragma HLS ARRAY_PARTITION variable=layer10_out complete dim=0

    layer11_t layer11_out[84];
    #pragma HLS ARRAY_PARTITION variable=layer11_out complete dim=0

    layer12_t layer12_out[84];
    #pragma HLS ARRAY_PARTITION variable=layer12_out complete dim=0

    layer13_t layer13_out[10];
    #pragma HLS ARRAY_PARTITION variable=layer13_out complete dim=0

    nnet::conv_2d_cl<input_t, layer2_t, config2>(input_layer, layer2_out, w2, b2); // conv2d

    nnet::relu<layer2_t, layer3_t, relu_config3>(layer2_out, layer3_out); // conv2d_relu

    nnet::pooling2d_cl<layer3_t, layer4_t, config4>(layer3_out, layer4_out); // average_pooling2d

    nnet::conv_2d_cl<layer4_t, layer5_t, config5>(layer4_out, layer5_out, w5, b5); // conv2d_1

    nnet::relu<layer5_t, layer6_t, relu_config6>(layer5_out, layer6_out); // conv2d_1_relu

    nnet::pooling2d_cl<layer6_t, layer7_t, config7>(layer6_out, layer7_out); // average_pooling2d_1

    nnet::dense<layer7_t, layer9_t, config9>(layer8_out, layer9_out, w9, b9); // dense

    nnet::relu<layer9_t, layer10_t, relu_config10>(layer9_out, layer10_out); // dense_relu

    nnet::dense<layer10_t, layer11_t, config11>(layer10_out, layer11_out, w11, b11); // dense_1

    nnet::relu<layer11_t, layer12_t, relu_config12>(layer11_out, layer12_out); // dense_1_relu

    nnet::dense<layer12_t, layer13_t, config13>(layer12_out, layer13_out, w13, b13); // dense_2

    nnet::softmax<layer13_t, result_t, softmax_config14>(layer13_out, layer14_out); // dense_2_softmax

}

