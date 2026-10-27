"""Test hls4ml bit-accurate accuracy for candidate ap_fixed precisions against the
full MNIST test set, without running Vitis HLS/Vivado at all.

hls_model.compile() builds the generated HLS C++ (ap_fixed emulation headers are
portable C++, no Xilinx tools needed) into a Python-callable bridge via a plain g++,
the same mechanism Vitis' own csim_design uses internally. hls_model.predict() then
runs inference through that bridge, rounding exactly as the eventual hardware would.
This lets many precision candidates be screened in minutes before committing to an
hours-long real synthesis run for just one of them.

Usage: python model/precision_sweep.py [--precisions ap_fixed<16,6> ap_fixed<12,6> ...] [--n 2000]

Note: granularity='name' (not 'model') is required so per-layer accumulator/bias/
weight/result precisions default to 'auto' (hls4ml bit-growth-widens accumulators
automatically). granularity='model' applies the tested precision as a single flat
type everywhere including accumulators, causing overflow/wraparound that tanks
accuracy independently of the precision being tested (verified: same <12,6> dropped
from 98.2% to 94.6%, <8,4> from 65.6% to 10.8% under granularity='model' -- an
artifact of the sweep, not a real result). ReuseFactor is intentionally left at
hls4ml's default here (not copied from export.py's per-layer values): RF changes
hardware implementation/resource cost, not the numeric result, so leaving it out
decouples this script from export.py's RF tuning.
"""
import argparse
import sys

sys.path.insert(0, 'model')
import torch

# torch.load must happen before `import hls4ml` (pulls in TensorFlow/oneDNN) -- mixing
# the import order segfaults, see project notes "export_pipeline".
model = torch.load('model/pruned_best.pth', weights_only=False)
model.eval()

import numpy as np
import torchvision
import torchvision.transforms as transforms

import hls4ml


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        '--precisions', nargs='+',
        default=['ap_fixed<16,6>', 'ap_fixed<13,6>', 'ap_fixed<12,6>', 'ap_fixed<11,6>', 'ap_fixed<10,6>'],
        help='ap_fixed<W,I> candidates to test',
    )
    parser.add_argument('--n', type=int, default=None, help='number of test images (default: full 10000)')
    args = parser.parse_args()

    test_dataset = torchvision.datasets.MNIST(
        root='./data', train=False, download=False,
        transform=transforms.Compose([
            transforms.Resize((32, 32)),
            transforms.ToTensor(),
            transforms.Normalize(mean=(0.1325,), std=(0.3105,)),
        ]),
    )
    n = args.n or len(test_dataset)
    loader = torch.utils.data.DataLoader(test_dataset, batch_size=n, shuffle=False)
    x, y = next(iter(loader))
    x_np = np.ascontiguousarray(x.numpy().astype(np.float32).reshape(n, -1))
    y_np = y.numpy()

    with torch.no_grad():
        torch_pred = model(x).argmax(dim=1).numpy()
    print(f"PyTorch FP32 reference accuracy on {n} test images: {(torch_pred == y_np).mean() * 100:.2f}%")
    print()
    print(f"{'Precision':16s} {'Accuracy':>10s} {'AgreeWithFP32':>14s}")

    for prec in args.precisions:
        tag = prec.replace('<', '_').replace('>', '').replace(',', '_')
        hls_config = hls4ml.utils.config_from_pytorch_model(
            model,
            input_shape=(1, 32, 32),
            granularity='name',
            backend='Vitis',
            channels_last_conversion='internal',
        )
        hls_config['Model']['Precision'] = prec

        hls_model = hls4ml.converters.convert_from_pytorch_model(
            model,
            hls_config=hls_config,
            output_dir=f'/tmp/precision_sweep/{tag}',
            project_name='myproject',
            backend='Vitis',
            io_type='io_stream',
            clock_period=14,
            part='xc7z010clg400-1',
        )
        hls_model.compile()

        hls_pred = hls_model.predict(x_np).argmax(axis=1)
        acc = (hls_pred == y_np).mean()
        agree = (hls_pred == torch_pred).mean()
        print(f"{prec:16s} {acc * 100:9.2f}% {agree * 100:13.2f}%")


if __name__ == '__main__':
    main()
