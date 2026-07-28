#include <iostream>

#include "myproject.h"
#include "parameters.h"

void myproject(
    hls::stream<input_t> &input_1,
    hls::stream<result_t> &layer14_out,
    conv2d_weight_t w2[150], 
    conv2d_1_weight_t w5[2400], 
    dense_weight_t w9[48000], 
    dense_bias_t b9[120], 
    dense_1_weight_t w11[10080], 
    dense_2_weight_t w13[840]
) {

    // hls-fpga-machine-learning insert IO
    #pragma HLS INTERFACE axis port=input_1,layer14_out 
    #pragma HLS INTERFACE bram port=w2,w5,w9,b9,w11,w13 
    #pragma HLS DATAFLOW 

#ifndef __SYNTHESIS__
    static bool loaded_weights = false;
    if (!loaded_weights) {
        // hls-fpga-machine-learning insert load weights
        nnet::load_weights_from_txt<conv2d_weight_t, 150>(w2, "w2.txt");
        nnet::load_weights_from_txt<conv2d_bias_t, 6>(b2, "b2.txt");
        nnet::load_weights_from_txt<conv2d_1_weight_t, 2400>(w5, "w5.txt");
        nnet::load_weights_from_txt<conv2d_1_bias_t, 16>(b5, "b5.txt");
        nnet::load_weights_from_txt<dense_weight_t, 48000>(w9, "w9.txt");
        nnet::load_weights_from_txt<dense_bias_t, 120>(b9, "b9.txt");
        nnet::load_weights_from_txt<dense_1_weight_t, 10080>(w11, "w11.txt");
        nnet::load_weights_from_txt<dense_1_bias_t, 84>(b11, "b11.txt");
        nnet::load_weights_from_txt<dense_2_weight_t, 840>(w13, "w13.txt");
        nnet::load_weights_from_txt<dense_2_bias_t, 10>(b13, "b13.txt");
        loaded_weights = true;
    }
#endif

    // ****************************************
    // NETWORK INSTANTIATION
    // ****************************************

    // hls-fpga-machine-learning insert layers

    hls::stream<layer2_t> layer2_out("layer2_out");
    #pragma HLS STREAM variable=layer2_out depth=784
    nnet::conv_2d_cl<input_t, layer2_t, config2>(input_1, layer2_out, w2, b2); // conv2d

    hls::stream<layer3_t> layer3_out("layer3_out");
    #pragma HLS STREAM variable=layer3_out depth=784
    nnet::relu<layer2_t, layer3_t, relu_config3>(layer2_out, layer3_out); // conv2d_relu

    hls::stream<layer4_t> layer4_out("layer4_out");
    #pragma HLS STREAM variable=layer4_out depth=196
    nnet::pooling2d_cl<layer3_t, layer4_t, config4>(layer3_out, layer4_out); // average_pooling2d

    hls::stream<layer5_t> layer5_out("layer5_out");
    #pragma HLS STREAM variable=layer5_out depth=100
    nnet::conv_2d_cl<layer4_t, layer5_t, config5>(layer4_out, layer5_out, w5, b5); // conv2d_1

    hls::stream<layer6_t> layer6_out("layer6_out");
    #pragma HLS STREAM variable=layer6_out depth=100
    nnet::relu<layer5_t, layer6_t, relu_config6>(layer5_out, layer6_out); // conv2d_1_relu

    hls::stream<layer7_t> layer7_out("layer7_out");
    #pragma HLS STREAM variable=layer7_out depth=25
    nnet::pooling2d_cl<layer6_t, layer7_t, config7>(layer6_out, layer7_out); // average_pooling2d_1

    auto& layer8_out = layer7_out;
    hls::stream<layer9_t> layer9_out("layer9_out");
    #pragma HLS STREAM variable=layer9_out depth=1
    nnet::dense<layer7_t, layer9_t, config9>(layer8_out, layer9_out, w9, b9); // dense

    hls::stream<layer10_t> layer10_out("layer10_out");
    #pragma HLS STREAM variable=layer10_out depth=1
    nnet::relu<layer9_t, layer10_t, relu_config10>(layer9_out, layer10_out); // dense_relu

    hls::stream<layer11_t> layer11_out("layer11_out");
    #pragma HLS STREAM variable=layer11_out depth=1
    nnet::dense<layer10_t, layer11_t, config11>(layer10_out, layer11_out, w11, b11); // dense_1

    hls::stream<layer12_t> layer12_out("layer12_out");
    #pragma HLS STREAM variable=layer12_out depth=1
    nnet::relu<layer11_t, layer12_t, relu_config12>(layer11_out, layer12_out); // dense_1_relu

    hls::stream<layer13_t> layer13_out("layer13_out");
    #pragma HLS STREAM variable=layer13_out depth=1
    nnet::dense<layer12_t, layer13_t, config13>(layer12_out, layer13_out, w13, b13); // dense_2

    nnet::softmax<layer13_t, result_t, softmax_config14>(layer13_out, layer14_out); // dense_2_softmax

}
