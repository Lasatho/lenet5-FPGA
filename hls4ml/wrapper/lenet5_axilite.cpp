#include "lenet5_axilite.h"

static void read_image(const pixel_word_t img[N_PIXELS], hls::stream<input_t> &x) {
read_loop:
    for (int i = 0; i < N_PIXELS; i++) {
        #pragma HLS PIPELINE II=1
        input_t pack;
        pack[0].range() = img[i].range(pixel_t::width - 1, 0);
        x.write(pack);
    }
}

static void write_logits(hls::stream<result_t> &y, logit_word_t logits[N_CLASSES]) {
    result_t pack = y.read();
write_loop:
    for (int j = 0; j < N_CLASSES; j++) {
        #pragma HLS PIPELINE II=1
        // via a signed integer of the same width, so the assignment sign-extends
        // (the bare core leaves bits 31..28 of each logit field zero)
        ap_int<logit_t::width> raw;
        raw.range() = pack[j].range();
        logits[j] = raw;
    }
}

void lenet5_axilite(const pixel_word_t img[N_PIXELS], logit_word_t logits[N_CLASSES]) {
    #pragma HLS INTERFACE s_axilite port=img bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=logits bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl
    #pragma HLS DATAFLOW

    hls::stream<input_t> x("x");
    hls::stream<result_t> y("y");

    read_image(img, x);
    myproject(x, y);
    write_logits(y, logits);
}
