import torch

# load trained model (must happen before importing hls4ml/tensorflow: torch.load
# unpickles LeNet5, which pulls in torchvision's native ops; if tensorflow/oneDNN is
# already loaded in the process first, registering those native ops segfaults)
model = torch.load('model/pruned_best.pth', weights_only=False)
model.eval()

import hls4ml

# configure hls4ml
hls_config = hls4ml.utils.config_from_pytorch_model(
    model,
    input_shape=(1, 32, 32),
    granularity='name',
    backend='Vitis',
)
hls_config['Model']['Precision'] = 'ap_fixed<16,6>'
hls_config['Model']['Strategy'] = 'Resource'
hls_config['Model']['ReuseFactor'] = 8000
hls_config['Model']['BramFactor'] = 100

# conv layers untouched by pruning, keep full serialization (1 DSP each, valid RF ==
# n_in*n_out for these shapes).
hls_config['LayerName']['layer1_0']['ReuseFactor'] = 150
hls_config['LayerName']['layer2_0']['ReuseFactor'] = 2400

# fc/fc1/fc2 shrank with pruning (400->36->25->10). hls4ml Resource-strategy dense
# requires RF in divisors(n_in) U {n_in*d : d divides n_out} (nnet_dense_resource.h
# rf_leq_nin / rf_gt_nin cases). DSP budget has huge headroom (was 12/80 pre-pruning)
# and LUT/FF are the real constraint, so favor lower RF / more DSP parallelism here
# instead of proportionally scaling the old (much more serialized) values.
# fc: n_in=400, n_out=36 -> RF=800 (18 DSP)
hls_config['LayerName']['fc']['ReuseFactor'] = 800
# fc1: n_in=36, n_out=25 -> RF=36 (25 DSP)
hls_config['LayerName']['fc1']['ReuseFactor'] = 36
# fc2: n_in=25, n_out=10 -> RF=25 (10 DSP)
hls_config['LayerName']['fc2']['ReuseFactor'] = 25
# Total estimate: 1+1+18+25+10 = 55/80 DSP (69%), comfortable P&R margin.


# convert to HLS project
hls_model = hls4ml.converters.convert_from_pytorch_model(
    model,
    hls_config=hls_config,
    output_dir='../hls4ml/lenet5_project',
    project_name='myproject',
    backend='Vitis',
    io_type='io_stream',
    clock_period=14
)

# plot model architecture
hls4ml.utils.plot_model(hls_model, show_shapes=True, show_precision=True, to_file='hls4ml_model.png')
print("hls4ml compatibility check passed")

# generate C++ HLS code
hls_model.compile()
print("HLS project generated at ../hls4ml/lenet5_project")
