# Stand-in for "xsct jtag.tcl load|read" in the host test: works on the DDR file that
# emu.cpp maps at 0x10000000. Called by send_image.py as  python fake_xsct.py <cmd> ...
# (send_image.py --xsct python --jtag-tcl fake_xsct.py).

import os
import sys

DDR_BASE = 0x10000000
DDR_IMAGE_BASE, DDR_LABEL_BASE, DDR_RESULT_BASE = 0x10000000, 0x11400000, 0x11500000
RESULT_BYTES = 48

ddr = os.environ['EMU_DDR_FILE']
cmd = sys.argv[1]
with open(ddr, 'r+b') as f:
    if cmd == 'load':
        for path, addr in ((sys.argv[2], DDR_IMAGE_BASE), (sys.argv[3], DDR_LABEL_BASE)):
            f.seek(addr - DDR_BASE)
            f.write(open(path, 'rb').read())
    elif cmd == 'read':
        f.seek(DDR_RESULT_BASE - DDR_BASE)
        open(sys.argv[2], 'wb').write(f.read(int(sys.argv[3]) * RESULT_BYTES))
    else:
        sys.exit(f'unknown command {cmd}')
