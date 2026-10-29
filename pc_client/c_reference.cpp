// Bit-exact reference for the hardware test, built with g++ and the hls4ml ap_types
// (no Vitis). Reads the output of prepare_dataset.py from pc_client/data/:
// 1. checks for all 256 pixel levels that floor(x * 2^6) in prepare_dataset.py equals the
//    float -> ap_fixed<11,5> conversion of hls4ml (nnet::convert_data / copy_data),
// 2. runs every image through the C model myproject() with the raw words fed in like
//    lenet5_axilite's read_image(), writes the logits and reports the accuracy,
// 3. compares the first images with tb_output_predictions.dat (hls_model.predict()).
//
//   g++ -O2 -std=c++14 -I hls4ml/lenet5_project/firmware \
//       -I hls4ml/lenet5_project/firmware/ap_types \
//       pc_client/c_reference.cpp hls4ml/lenet5_project/firmware/myproject.cpp \
//       -o pc_client/data/c_reference
//   pc_client/data/c_reference [<max_images>]          # from the repo root
//
// Output: pc_client/data/ref_logits.bin, int32 little-endian, N x 10, raw ap_fixed<28,16>
// bits sign-extended (value = word / 2^12), the same words the wrapper returns.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "myproject.h"

static const int N_PIXELS = 32 * 32;
static const int N_CLASSES = 10;
typedef input_t::value_type pixel_t;   // ap_fixed<11,5>
typedef result_t::value_type logit_t;  // ap_fixed<28,16>

static const std::string DATA = "pc_client/data/";
static const std::string FIRMWARE = "hls4ml/lenet5_project/firmware";
static const std::string TB_PRED = "hls4ml/lenet5_project/tb_data/tb_output_predictions.dat";

template <class T> static std::vector<T> read_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        std::cout << "ERROR: Unable to open " << path << std::endl;
        std::exit(1);
    }
    std::vector<T> v(f.tellg() / sizeof(T));
    f.seekg(0);
    f.read(reinterpret_cast<char *>(v.data()), v.size() * sizeof(T));
    return v;
}

static int check_levels() {
    std::vector<char> raw = read_file<char>(DATA + "levels.bin");
    const size_t rec = sizeof(float) + sizeof(int16_t);
    int bad = 0;
    for (size_t i = 0; i < raw.size() / rec; i++) {
        float v;
        int16_t q;
        std::memcpy(&v, &raw[i * rec], sizeof(v));
        std::memcpy(&q, &raw[i * rec + sizeof(v)], sizeof(q));
        pixel_t p = pixel_t(v);
        ap_int<pixel_t::width> bits;
        bits.range() = p.range();
        if (bits.to_int() != q) {
            std::printf("level %zu: x = %.9g, prepare_dataset.py %d, ap_fixed %d\n", i, v, q, bits.to_int());
            bad++;
        }
    }
    std::cout << "INFO: " << raw.size() / rec << " pixel levels, quantization mismatches: " << bad << std::endl;
    return bad;
}

int main(int argc, char **argv) {
    if (check_levels())
        return 1;

    std::vector<int16_t> img = read_file<int16_t>(DATA + "mnist_test_q.bin");
    std::vector<uint8_t> labels = read_file<uint8_t>(DATA + "mnist_test_labels.bin");
    size_t n = labels.size();
    if (img.size() != n * N_PIXELS) {
        std::cout << "ERROR: " << img.size() << " pixel words for " << n << " labels" << std::endl;
        return 1;
    }
    if (argc > 1)
        n = std::min(n, (size_t)std::atoi(argv[1]));

    std::ifstream fpr(TB_PRED);
    std::ofstream fout(DATA + "ref_logits.bin", std::ios::binary);
    // myproject() loads its weights from weights/*.txt relative to the working directory
    // on the first call (C simulation path); all other files are open at this point
    if (chdir(FIRMWARE.c_str()) != 0) {
        std::cout << "ERROR: Unable to change to " << FIRMWARE << std::endl;
        return 1;
    }
    int correct = 0, n_pred = 0, mismatch_pred = 0;
    std::string pline;
    for (size_t k = 0; k < n; k++) {
        hls::stream<input_t> x("x");
        hls::stream<result_t> y("y");
        for (int i = 0; i < N_PIXELS; i++) {
            input_t pack;
            pack[0].range() = ap_int<16>(img[k * N_PIXELS + i]).range(pixel_t::width - 1, 0);
            x.write(pack);
        }
        myproject(x, y);
        result_t out = y.read();

        int32_t words[N_CLASSES];
        int best = 0;
        for (int j = 0; j < N_CLASSES; j++) {
            ap_int<logit_t::width> raw;
            raw.range() = out[j].range();
            words[j] = raw.to_int();
            if (out[j] > out[best])
                best = j;
        }
        fout.write(reinterpret_cast<const char *>(words), sizeof(words));
        correct += best == labels[k];

        // %.8f in tb_output_predictions.dat, logit LSB is 2^-12
        if (fpr.is_open() && std::getline(fpr, pline)) {
            std::istringstream ss(pline);
            for (int j = 0; j < N_CLASSES; j++) {
                double p;
                ss >> p;
                if (std::fabs(out[j].to_double() - p) > 1e-6)
                    mismatch_pred++;
            }
            n_pred++;
        }
        if ((k + 1) % 1000 == 0)
            std::cout << "INFO: " << k + 1 << " images" << std::endl;
    }

    std::printf("INFO: %zu images, accuracy %.2f %% (%d correct)\n", n, 100.0 * correct / n, correct);
    std::cout << "INFO: first " << n_pred << " images vs. tb_output_predictions.dat, logit mismatches: "
              << mismatch_pred << std::endl;
    return (n_pred == 0 || mismatch_pred) ? 1 : 0;
}
