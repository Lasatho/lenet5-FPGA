# Preprocesses the MNIST test set once on the PC into the raw input words of the FPGA
# core, so neither the PC client nor the bare-metal software has to preprocess.
#
#   python pc_client/prepare_dataset.py        # from the repo root
#
# Same transform as the testbench data in model/export.py (Resize to 32x32, ToTensor,
# Normalize with the test-set constants), then float -> ap_fixed<11,5> like hls4ml
# (ap_fixed default AP_TRN: floor(x * 2^6)), sign-extended to int16.
#
# Output in pc_client/data/:
#   mnist_test_q.bin       int16 little-endian, N x 1024, row-major 32x32 per image
#   mnist_test_labels.bin  uint8, N
#   levels.bin             256 x (float32 normalized value, int16 quantized word) for the
#                          uint8 pixel levels 0..255, checked by c_reference.cpp

import os

import numpy as np
import torch
import torchvision
import torchvision.transforms as transforms

# keep in sync with model/export.py (tb_data) and model/train.py (test_dataset)
MEAN, STD = 0.1325, 0.3105
FRAC_BITS = 6          # ap_fixed<11,5>
WORD_MIN, WORD_MAX = -2**10, 2**10 - 1

out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
os.makedirs(out_dir, exist_ok=True)

normalize = transforms.Normalize(mean=(MEAN,), std=(STD,))
test_dataset = torchvision.datasets.MNIST(
    root='./data', train=False, download=False,
    transform=transforms.Compose([
        transforms.Resize((32, 32)),
        transforms.ToTensor(),
        normalize,
    ]),
)


def quantize(x):
    q = np.floor(x.astype(np.float64) * 2**FRAC_BITS)
    assert q.min() >= WORD_MIN and q.max() <= WORD_MAX, (q.min(), q.max())
    return q.astype(np.int16)


# Resize works on the PIL image and returns uint8 again, so every normalized pixel is one
# of 256 levels; computed with the same tensor ops as ToTensor + Normalize
levels_u8 = torch.arange(256, dtype=torch.uint8).view(1, 1, 256)
levels = normalize(levels_u8.float().div(255)).numpy().reshape(-1).astype(np.float32)
levels_q = quantize(levels)

x = np.stack([test_dataset[i][0].numpy().reshape(-1) for i in range(len(test_dataset))])
labels = np.array(test_dataset.targets, dtype=np.uint8)
assert x.dtype == np.float32 and x.shape == (len(test_dataset), 32 * 32)
assert np.isin(x, levels).all(), 'pixel values outside the 256 uint8 levels'
x_q = quantize(x)

x_q.astype('<i2').tofile(os.path.join(out_dir, 'mnist_test_q.bin'))
labels.tofile(os.path.join(out_dir, 'mnist_test_labels.bin'))
with open(os.path.join(out_dir, 'levels.bin'), 'wb') as f:
    for v, q in zip(levels, levels_q):
        f.write(np.float32(v).astype('<f4').tobytes() + np.int16(q).astype('<i2').tobytes())

print(f'{len(x_q)} images, words {x_q.min()}..{x_q.max()}, written to {out_dir}')
