# Error paths of the UART protocol against the emulated board (run_host_test.sh):
# corrupted crc, truncated request, unknown command, wrong length; the board must answer
# each with the matching status and stay responsive.
#
#   python test_protocol_errors.py <port>

import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'pc_client'))
from send_image import CMD_INFER, CMD_PING, STATUS, Board, crc16  # noqa: E402

board = Board(sys.argv[1], 921600, timeout=2.0)
failed = 0


def expect(name, frame, status_expected):
    global failed
    board.ser.write(frame)
    _, status, _ = board._response()
    ok = status == status_expected
    failed += not ok
    print(f'{name}: status {STATUS.get(status, status)} ({"ok" if ok else "FAILED"})')
    _, status, _ = board.request(CMD_PING)
    failed += status != 0


def frame(cmd, payload, crc_xor=0):
    body = struct.pack('<BH', cmd, len(payload)) + payload
    return bytes([0xA5]) + body + struct.pack('<H', crc16(body) ^ crc_xor)


image = bytes(2048)
expect('crc corrupted', frame(CMD_INFER, image, crc_xor=0x0001), 1)
expect('truncated request', frame(CMD_INFER, image)[:1000], 5)
expect('unknown command', frame(ord('X'), b''), 3)
expect('wrong length', frame(CMD_INFER, image[:100]), 2)
expect('length > max', bytes([0xA5]) + struct.pack('<BH', CMD_INFER, 5000), 2)
time.sleep(0.1)   # board resyncs on the next sync byte
board.ser.reset_input_buffer()
sys.exit(1 if failed else 0)
