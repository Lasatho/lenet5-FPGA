import torch

# load trained model (must happen before importing hls4ml/tensorflow: torch.load
# unpickles LeNet5, which pulls in torchvision's native ops; if tensorflow/oneDNN is
# already loaded in the process first, registering those native ops segfaults)
model = torch.load('model/pruned_best.pth', weights_only=False)
model.eval()

import hls4ml

# hls4ml already ships an optimizer (RemoveTransposeBeforeFlatten in
# hls4ml.model.optimizer.passes.convert_to_channels_last) that removes the pre-flatten
# transpose entirely by permuting the following Dense layer's weight columns instead --
# mathematically identical, zero hardware cost. But its match() hardcodes
# IOType == 'io_parallel', so it never fires for us (io_stream is required here, see
# project notes "synthese_laeufe_pruned", Lauf I). Verified this restriction is not load-bearing:
# with io_stream, patching match() to also accept the Transpose->Reshape->Dense pattern
# produces a graph with the Transpose node gone entirely, and hls_model.predict() gives
# 100% bit-identical predictions (2000/2000 MNIST test images) vs. the unpatched
# version -- confirmed via bit-accurate emulation, same technique as
# model/precision_sweep.py. This eliminates the pre-flatten transpose's ~9.2K LUT /
# ~4.5K FF entirely (was the single largest non-fc contributor after the <11,5>
# synthesis run, see project notes "synthese_laeufe_pruned").
from hls4ml.model.optimizer.passes.convert_to_channels_last import RemoveTransposeBeforeFlatten


def _match_allow_io_stream(self, node):
    from hls4ml.model.layers import Dense, Reshape, Transpose

    if hasattr(node, '_channels_last_keep_transpose') and node._channels_last_keep_transpose:
        return False
    if isinstance(node, Reshape):
        input_node = node.get_input_node()
        output_nodes = node.get_output_nodes()
        if (
            len(node.get_attr('target_shape')) == 1
            and isinstance(input_node, Transpose)
            and len(output_nodes) == 1
            and isinstance(output_nodes[0], Dense)
        ):
            return True
    return False


RemoveTransposeBeforeFlatten.match = _match_allow_io_stream

# configure hls4ml
hls_config = hls4ml.utils.config_from_pytorch_model(
    model,
    input_shape=(1, 32, 32),
    granularity='name',
    backend='Vitis',
    # input is single-channel (C=1): channels-first (1,H,W) and channels-last (H,W,1)
    # are byte-identical, so the input transpose 'full' would insert is pure overhead
    # (measured: 16.4K FF / 2.9K LUT for a no-op). 'internal' skips it; only the
    # pre-flatten transpose (16 channels, actually needed) remains.
    channels_last_conversion='internal',
)
# Precision swept via model/precision_sweep.py (hls4ml bit-accurate Python emulation,
# no Vitis needed) against full MNIST test set. Two independent failure modes found:
# - fractional bits (W-I) < 6: rounding too coarse, collapses to ~65% (e.g. <10,6>,
#   <8,4>) or worse.
# - integer bits (I) < 5: range too tight even with frac=6, overflows/saturates and
#   collapses anyway (<10,4>, frac=6 but I=4, still only 65.99%).
# So I=5, frac=6 (W=11) is the minimum safe corner: <11,5> scored 98.72% (vs 98.76%
# for <12,6>, i.e. within noise) at 1 bit narrower. <12,6> and <11,5> both tested
# safe; <11,5> got synthesis (Lauf E) under budget on all four resources, but
# place_design still fails on control-set/slice packing (Lauf F/G/H, io_parallel dead
# end too, see project notes "synthese_laeufe_pruned").
# Tried <10,5> twice (Lauf J/K, see project notes "quantisierung"): DSP collapses 78->0, LUT jumps
# 96.4%->148.75% -- confirmed this happens in Vivado's synth_design itself (DSP
# already 0 in report_utilization taken immediately after synth_design, before any
# opt_design pass runs -- ruled out -propconst/constant-folding as the cause).
# Generated Verilog for the narrow multiply is plain `assign m = a * b`, no use_dsp
# attribute, no hard DSP48E1 instantiation -- Vivado's own inference heuristic simply
# declines DSP at this bit width for reasons not further diagnosed (would need a
# direct Verilog diff against the <11,5> case, not a TCL-level lever). Two failed
# attempts is enough -- precision below <11,5> is not viable here. <11,5> stays the
# floor (both for accuracy and for DSP inference).
hls_config['Model']['Precision'] = 'ap_fixed<11,5>'
hls_config['Model']['Strategy'] = 'Resource'
hls_config['Model']['ReuseFactor'] = 8000
# BramFactor is hls4ml's threshold for moving weight arrays out of the IP as external
# BRAM ports (hls4ml/model/graph.py, model_bf). Default 1e9 keeps all weights inside
# the IP (see project notes "export_pipeline").
hls_config['Model']['BramFactor'] = 1_000_000_000

# conv layers untouched by pruning. layer1_0 (n_in=25, n_out=6) was at RF=150
# (=n_in*n_out, full serialization, 1 DSP) -- that's > n_in, so it was silently in the
# expensive rf_gt_nin regime the whole time (same pattern as fc's Lauf A bug), just
# never noticed since DSP cost there is only 1. RF=25 (=n_in) crosses into rf_leq_nin
# for +5 DSP (1->6) (see project notes "reusefactor_regeln"). layer2_0 (n_in=150,
# n_out=16) at RF=2400 sat in rf_gt_nin as well; RF=150 (=n_in) costs +15 DSP, paid for
# by moving fc1/fc2 below. Net effect: layer2_0 -1.2K LUT / -2.1K FF, latency 6.6 ms ->
# 0.44 ms (Lauf O, see project notes "experimente_lut_reduktion").
hls_config['LayerName']['layer1_0']['ReuseFactor'] = 25
hls_config['LayerName']['layer2_0']['ReuseFactor'] = 150

# fc/fc1/fc2 shrank with pruning (400->36->25->10). hls4ml Resource-strategy dense
# requires RF in divisors(n_in) U {n_in*d : d divides n_out} (nnet_dense_resource.h
# rf_leq_nin / rf_gt_nin cases). RF <= n_in hits the cheap rf_leq_nin path; RF > n_in
# (even if "lower" than a fully-serialized baseline) still falls into rf_gt_nin, which
# carries heavy reuse-loop control/address logic (measured: fc at RF=800 > n_in=400
# cost 24.1K FF/20.1K LUT for 18 DSP, vs. fc1 at RF=36=n_in costing only 4.1K FF/1.8K
# LUT for 25 DSP). fc stays at RF == n_in. fc1/fc2 are small enough that rf_gt_nin
# costs less than it frees for layer2_0 (Lauf O).
# fc: n_in=400, n_out=36 -> RF=400 (36 DSP)
hls_config['LayerName']['fc']['ReuseFactor'] = 400
# fc1: n_in=36, n_out=25 -> RF=180 (5 DSP, rf_gt_nin)
hls_config['LayerName']['fc1']['ReuseFactor'] = 180
# fc2: n_in=25, n_out=10 -> RF=250 (1 DSP, rf_gt_nin)
hls_config['LayerName']['fc2']['ReuseFactor'] = 250
# Total: 6+16+36+5+1 = 64/80 DSP planned (63 after Vivado synthesis).

# Pre-ReLU outputs of the dense layers default to the full accumulator width ('auto',
# 29-32 bits), which widens the streams into the ReLUs ~3x. Narrow them to the
# activation width with saturation (bit-accurate emulation: 98.76% vs 98.72% with
# 'auto'). Not applied to layer1_0/layer2_0: an explicit conv result precision stops
# hls4ml from folding the following BatchNorm. WRAP instead of SAT crashes Vitis HLS
# 2024.2 in csynth (see project notes "experimente_lut_reduktion").
for name in ['fc', 'fc1']:
    hls_config['LayerName'][name]['Precision']['result'] = 'ap_fixed<11,5,TRN,SAT>'


# convert to HLS project
hls_model = hls4ml.converters.convert_from_pytorch_model(
    model,
    hls_config=hls_config,
    output_dir='hls4ml/lenet5_project',
    project_name='myproject',
    backend='Vitis',
    # io_parallel tried and aborted (Lauf I, see project notes "synthese_laeufe_pruned"): even on the pruned+quantized
    # net (400->36->25->10, way smaller than Lauf 1's 400->120->84->10), Vitis HLS
    # csim/csynth alone hit 16GB RAM after 20 minutes with a clear trend toward the
    # same blowup as Lauf 1 (13h, 24.8GB RAM abort). Back to io_stream -- its DATAFLOW
    # per-layer FSM overhead is the suspected control-set driver behind the
    # place_design failure (Lauf F/G/H), but it's the only io_type that actually
    # synthesizes here.
    io_type='io_stream',
    # 20 ns (50 MHz): at 14 ns the saturating cast in fc's pipelined ReuseLoop needed an
    # extra stage, duplicating all 400 input registers. 30 ns was worse again; the
    # scheduler's register structure is not monotonic in the period (Lauf R/R'/T).
    clock_period=20,
    part='xc7z010clg400-1'
)

# plot model architecture
hls4ml.utils.plot_model(hls_model, show_shapes=True, show_precision=True, to_file='hls4ml_model.png')
print("hls4ml compatibility check passed")

# generate C++ HLS code
hls_model.compile()

# io_stream dense collects its input stream into an array that hls4ml partitions
# completely, i.e. n_in registers plus a n_in:1 mux in front of the reuse loop. With
# RF >= n_in only one element is read per cycle, so cyclic banks matching the stream
# width suffice (fc: 16 banks x 25 words in LUTRAM; fc1/fc2: data_T::size == n_in, i.e.
# unchanged). Saves ~8.6K FF / ~4K LUT on fc and made place_design succeed (Lauf U, see
# project notes "experimente_lut_reduktion"). Patched after compile() because
# write()/compile() regenerate nnet_utils.
dense_stream_h = 'hls4ml/lenet5_project/firmware/nnet_utils/nnet_dense_stream.h'
with open(dense_stream_h) as f:
    src = f.read()
old = '#pragma HLS ARRAY_PARTITION variable=data complete'
assert src.count(old) == 1, 'nnet_dense_stream.h changed, partition patch needs review'
with open(dense_stream_h, 'w') as f:
    f.write(src.replace(old, '#pragma HLS ARRAY_PARTITION variable=data cyclic factor=data_T::size'))

# testbench data for csim/cosim (tb_data was empty, the testbench then falls back to
# 5 all-zero inputs): first TB_SAMPLES MNIST test images with the training
# preprocessing, expected outputs from the bit-accurate emulation
TB_SAMPLES = 100
import numpy as np
import torchvision
import torchvision.transforms as transforms

test_dataset = torchvision.datasets.MNIST(
    root='./data', train=False, download=False,
    transform=transforms.Compose([
        transforms.Resize((32, 32)),
        transforms.ToTensor(),
        transforms.Normalize(mean=(0.1325,), std=(0.3105,)),
    ]),
)
x_tb = np.stack([test_dataset[i][0].numpy().reshape(-1) for i in range(TB_SAMPLES)]).astype(np.float32)
y_tb = hls_model.predict(np.ascontiguousarray(x_tb))
np.savetxt('hls4ml/lenet5_project/tb_data/tb_input_features.dat', x_tb, fmt='%.8f')
np.savetxt('hls4ml/lenet5_project/tb_data/tb_output_predictions.dat', y_tb, fmt='%.8f')
print("HLS project generated at hls4ml/lenet5_project")
