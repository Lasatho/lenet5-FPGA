# PC client for the bare-metal app (vitis/app): sends preprocessed MNIST test images over
# UART and compares the logits bit-exactly with the C reference.
#
#   python send_image.py --port COM5 ping
#   python send_image.py --port COM5 infer 0
#   python send_image.py --port COM5 batch [--n 10000] [--out results.csv]
#   python send_image.py --port COM5 ddr [--n 10000]      # test set via JTAG into DDR
#
# Expects pc_client/data/ from prepare_dataset.py and c_reference (ref_logits.bin).
# Protocol in vitis/app/src/protocol.h (keep in sync). Runs on the machine with the
# board's COM port; the ddr command also needs xsct and the app booted via jtag.tcl.

import argparse
import binascii
import os
import struct
import subprocess
import sys
import time

import numpy as np
import serial

SYNC_REQUEST, SYNC_RESPONSE = 0xA5, 0x5A
CMD_PING, CMD_INFER, CMD_DDR = ord('P'), ord('I'), ord('D')
STATUS = {0: 'OK', 1: 'CRC', 2: 'LENGTH', 3: 'COMMAND', 4: 'IP_TIMEOUT', 5: 'FRAME'}
TRANSMISSION_ERRORS = (1, 5)
N_PIXELS, N_CLASSES = 1024, 10
LOGIT_FRAC_BITS = 12

PING_REPLY = struct.Struct('<HII')
INFER_REPLY = struct.Struct('<10iBII')
DDR_REPLY = struct.Struct('<IIIIQQQ')
DDR_RESULT = np.dtype([('logits', '<i4', N_CLASSES), ('t_core', '<u4'), ('t_total', '<u4')])

script_dir = os.path.dirname(os.path.abspath(__file__))


class ProtocolError(Exception):
    pass


def crc16(data):
    return binascii.crc_hqx(data, 0xFFFF)


class Board:
    def __init__(self, port, baud, timeout):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.ser.reset_input_buffer()

    def request(self, cmd, payload=b'', timeout=None):
        body = struct.pack('<BH', cmd, len(payload)) + payload
        if timeout is not None:
            self.ser.timeout = timeout
        self.ser.write(bytes([SYNC_REQUEST]) + body + struct.pack('<H', crc16(body)))
        return self._response()

    def _read(self, n):
        data = self.ser.read(n)
        if len(data) != n:
            raise ProtocolError(f'timeout ({len(data)}/{n} bytes)')
        return data

    def _response(self):
        while self._read(1)[0] != SYNC_RESPONSE:
            pass
        head = self._read(4)
        cmd, status, length = struct.unpack('<BBH', head)
        payload = self._read(length)
        (crc,) = struct.unpack('<H', self._read(2))
        if crc != crc16(head + payload):
            raise ProtocolError('response crc mismatch')
        return cmd, status, payload

    def resync(self):
        time.sleep(0.1)   # the board drops a partial request after 50 ms without bytes
        self.ser.reset_input_buffer()


def load_data(data_dir):
    images = np.fromfile(os.path.join(data_dir, 'mnist_test_q.bin'), '<i2').reshape(-1, N_PIXELS)
    labels = np.fromfile(os.path.join(data_dir, 'mnist_test_labels.bin'), np.uint8)
    ref = np.fromfile(os.path.join(data_dir, 'ref_logits.bin'), '<i4').reshape(-1, N_CLASSES)
    return images, labels, ref


def check_status(status, what=''):
    if status != 0:
        sys.exit(f'ERROR: board status {STATUS.get(status, status)}{what}')


def ping(board):
    _, status, payload = board.request(CMD_PING)
    check_status(status)
    return PING_REPLY.unpack(payload)   # protocol version, timer Hz, baud


def infer(board, image, retries=3):
    """Returns (logits, argmax, t_core, t_total, retried transmission errors)."""
    errors = 0
    while True:
        try:
            _, status, payload = board.request(CMD_INFER, image.astype('<i2').tobytes())
            if status in TRANSMISSION_ERRORS:
                raise ProtocolError(f'board status {STATUS[status]}')
            check_status(status)
            *logits, best, t_core, t_total = INFER_REPLY.unpack(payload)
            return np.array(logits, np.int32), best, t_core, t_total, errors
        except ProtocolError as e:
            errors += 1
            if errors > retries:
                sys.exit(f'ERROR: {e}, giving up after {retries} retries')
            board.resync()


def report_mismatch(logits, ref):
    mismatch = np.flatnonzero((logits != ref).any(axis=1))
    n = len(logits)
    print(f'bit-exact vs. ref_logits.bin: {n - len(mismatch)}/{n}'
          + (f', first mismatches: {mismatch[:10].tolist()}' if len(mismatch) else ''))
    return mismatch


def report_time(name, ticks, timer_hz):
    us = np.asarray(ticks, np.float64) / timer_hz * 1e6
    print(f'{name}: mean {us.mean():.2f} us, min {us.min():.2f} us, max {us.max():.2f} us')


def cmd_infer(board, args, timer_hz):
    images, labels, ref = load_data(args.data)
    k = args.index
    logits, best, t_core, t_total, _ = infer(board, images[k])
    print('logits:', ' '.join(f'{v / 2**LOGIT_FRAC_BITS:.4f}' for v in logits))
    print(f'argmax {best}, label {labels[k]}, bit-exact {bool((logits == ref[k]).all())}')
    print(f't_core {t_core / timer_hz * 1e6:.2f} us, t_total {t_total / timer_hz * 1e6:.2f} us')
    return 0 if (logits == ref[k]).all() else 1


def cmd_batch(board, args, timer_hz):
    images, labels, ref = load_data(args.data)
    n = min(args.n, len(images))
    logits = np.zeros((n, N_CLASSES), np.int32)
    best = np.zeros(n, np.int64)
    t_core = np.zeros(n, np.uint32)
    t_total = np.zeros(n, np.uint32)
    tx_errors = 0
    start = time.perf_counter()
    for k in range(n):
        logits[k], best[k], t_core[k], t_total[k], e = infer(board, images[k])
        tx_errors += e
        if (k + 1) % 500 == 0:
            print(f'{k + 1}/{n}', flush=True)
    wall = time.perf_counter() - start

    print(f'{n} images in {wall:.1f} s, transmission errors (retried): {tx_errors}')
    mismatch = report_mismatch(logits, ref[:n])
    print(f'accuracy {100 * (best == labels[:n]).mean():.2f} %')
    report_time('t_core ', t_core, timer_hz)
    report_time('t_total', t_total, timer_hz)
    if args.out:
        exact = np.ones(n, bool)
        exact[mismatch] = False
        with open(args.out, 'w') as f:
            f.write('index,label,argmax,bit_exact,t_core,t_total\n')
            for k in range(n):
                f.write(f'{k},{labels[k]},{best[k]},{int(exact[k])},{t_core[k]},{t_total[k]}\n')
    return 1 if len(mismatch) else 0


def cmd_ddr(board, args, timer_hz):
    images, labels, ref = load_data(args.data)
    n = min(args.n, len(images))
    xsct = [args.xsct, args.jtag_tcl]
    print('loading test set into DDR via JTAG ...', flush=True)
    subprocess.run(xsct + ['load', os.path.join(args.data, 'mnist_test_q.bin'),
                           os.path.join(args.data, 'mnist_test_labels.bin')], check=True)
    board.resync()

    _, status, payload = board.request(CMD_DDR, struct.pack('<I', n), timeout=10 + n * 0.01)
    r_n, correct, t_min, t_max, t_core_sum, t_total_sum, t_run = DDR_REPLY.unpack(payload)
    check_status(status, f' after {r_n} images')

    out = os.path.join(args.data, 'ddr_results.bin')
    subprocess.run(xsct + ['read', out, str(n)], check=True)
    res = np.fromfile(out, DDR_RESULT)[:n]

    us = 1e6 / timer_hz
    print(f'{r_n} images, accuracy {100 * correct / r_n:.2f} % (board)')
    mismatch = report_mismatch(res['logits'], ref[:n])
    print(f't_core : mean {t_core_sum / r_n * us:.2f} us, min {t_min * us:.2f} us, max {t_max * us:.2f} us')
    print(f't_total: mean {t_total_sum / r_n * us:.2f} us')
    print(f'run: {t_run / timer_hz:.3f} s, throughput {r_n / (t_run / timer_hz):.1f} images/s')
    return 1 if len(mismatch) else 0


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--port', required=True)
    p.add_argument('--baud', type=int, default=921600)
    p.add_argument('--data', default=os.path.join(script_dir, 'data'))
    p.add_argument('--xsct', default=r'C:\Xilinx-01\Vitis\2024.2\bin\xsct.bat')
    p.add_argument('--jtag-tcl', default=os.path.join(script_dir, '..', 'vitis', 'jtag.tcl'))
    sub = p.add_subparsers(dest='cmd', required=True)
    sub.add_parser('ping')
    sub.add_parser('infer').add_argument('index', type=int)
    p_batch = sub.add_parser('batch')
    p_batch.add_argument('--n', type=int, default=10000)
    p_batch.add_argument('--out')
    sub.add_parser('ddr').add_argument('--n', type=int, default=10000)
    args = p.parse_args()

    board = Board(args.port, args.baud, timeout=2.0)
    version, timer_hz, baud = ping(board)
    print(f'board: protocol {version}, timer {timer_hz} Hz, baud {baud}')

    commands = {'ping': lambda *_: 0, 'infer': cmd_infer, 'batch': cmd_batch, 'ddr': cmd_ddr}
    return commands[args.cmd](board, args, timer_hz)


if __name__ == '__main__':
    sys.exit(main())
