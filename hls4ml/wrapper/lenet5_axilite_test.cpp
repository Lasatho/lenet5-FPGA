#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lenet5_axilite.h"

// Testbench for lenet5_axilite(): per image the wrapper output is compared
// - bit-exactly against a direct call of the C model myproject() and
// - against the hls4ml emulation output in tb_output_predictions.dat.
// Returns non-zero on any mismatch, so csim/cosim fail.

static std::vector<float> parse_line(const std::string &line) {
    std::vector<float> v;
    std::istringstream ss(line);
    float f;
    while (ss >> f)
        v.push_back(f);
    return v;
}

int main() {
    std::ifstream fin("tb_data/tb_input_features.dat");
    std::ifstream fpr("tb_data/tb_output_predictions.dat");
#ifdef RTL_SIM
    std::string RESULTS_LOG = "tb_data/rtl_cosim_results.log";
#else
    std::string RESULTS_LOG = "tb_data/csim_results.log";
#endif
    std::ofstream fout(RESULTS_LOG);

    if (!fin.is_open() || !fpr.is_open()) {
        std::cout << "ERROR: Unable to open tb_data/tb_input_features.dat or tb_output_predictions.dat" << std::endl;
        return 1;
    }

    // %.8f in tb_output_predictions.dat, logit LSB is 2^-12
    const double PRED_TOL = 1e-6;
    int n = 0, mismatch_ref = 0, mismatch_pred = 0, bad_sext = 0;
    std::string iline, pline;
    while (std::getline(fin, iline) && std::getline(fpr, pline)) {
        std::vector<float> in = parse_line(iline);
        std::vector<float> pr = parse_line(pline);
        if (in.size() != (size_t)N_PIXELS || pr.size() != (size_t)N_CLASSES) {
            std::cout << "ERROR: line " << n << ": " << in.size() << " inputs, " << pr.size() << " predictions" << std::endl;
            return 1;
        }

        // same float -> ap_fixed<11,5> conversion as nnet::copy_data in the hls4ml testbench
        pixel_word_t img[N_PIXELS];
        hls::stream<input_t> x_ref("x_ref");
        for (int i = 0; i < N_PIXELS; i++) {
            input_t pack;
            pack[0] = pixel_t(in[i]);
            ap_int<pixel_t::width> raw;
            raw.range() = pack[0].range();
            img[i] = raw;
            x_ref.write(pack);
        }

        logit_word_t logits[N_CLASSES];
        lenet5_axilite(img, logits);

        hls::stream<result_t> y_ref("y_ref");
        myproject(x_ref, y_ref);
        result_t ref = y_ref.read();

        for (int j = 0; j < N_CLASSES; j++) {
            logit_t hw;
            hw.range() = logits[j].range(logit_t::width - 1, 0);
            if (logits[j] != logit_word_t(ap_int<logit_t::width>(logits[j].range(logit_t::width - 1, 0))))
                bad_sext++;
            if (hw != ref[j])
                mismatch_ref++;
            if (std::fabs(hw.to_double() - pr[j]) > PRED_TOL)
                mismatch_pred++;
            fout << hw << " ";
        }
        fout << std::endl;
        n++;
    }
    fout.close();

    std::cout << "INFO: " << n << " images, mismatches vs. myproject(): " << mismatch_ref
              << ", vs. tb_output_predictions.dat: " << mismatch_pred
              << ", logits not sign-extended: " << bad_sext << std::endl;
    std::cout << "INFO: Saved inference results to file: " << RESULTS_LOG << std::endl;

    if (n == 0 || mismatch_ref || mismatch_pred || bad_sext) {
        std::cout << "ERROR: Test failed" << std::endl;
        return 1;
    }
    std::cout << "INFO: Test PASSED" << std::endl;
    return 0;
}
