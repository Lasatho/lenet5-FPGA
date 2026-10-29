#!/bin/bash
# Host test of the bare-metal app and the PC client without board or Vitis: builds
# vitis/app/src/main.c with the stubs and emu.cpp (C model as IP, pty as UART) and runs
# send_image.py against it. Run from the repo root after prepare_dataset.py and
# c_reference (pc_client/data/).
#
#   vitis/app/host_test/run_host_test.sh [<n_batch>] [<n_ddr>]
set -euo pipefail

N_BATCH=${1:-100}
N_DDR=${2:-10000}
ROOT=$(pwd)
HT=$ROOT/vitis/app/host_test
FW=$ROOT/hls4ml/lenet5_project/firmware
PY=${PYTHON:-$ROOT/.venv/bin/python}
TMP=$(mktemp -d)
trap 'kill $EMU 2>/dev/null || true; rm -rf "$TMP"' EXIT

gcc -O2 -Wall -Wextra -c -I "$HT/stubs" vitis/app/src/main.c -o "$TMP/main.o"
g++ -O2 -std=c++14 -I "$HT/stubs" -I "$FW" -I "$FW/ap_types" -c "$HT/emu.cpp" -o "$TMP/emu.o"
g++ -O2 -std=c++14 -I "$FW" -I "$FW/ap_types" -c "$FW/myproject.cpp" -o "$TMP/myproject.o"
g++ "$TMP/main.o" "$TMP/emu.o" "$TMP/myproject.o" -o "$TMP/app"

export EMU_PTY_FILE=$TMP/pty EMU_DDR_FILE=$TMP/ddr EMU_FIRMWARE_DIR=$FW
"$TMP/app" &
EMU=$!
for _ in $(seq 50); do [ -s "$TMP/pty" ] && break; sleep 0.1; done
PORT=$(cat "$TMP/pty")

CLIENT=("$PY" pc_client/send_image.py --port "$PORT" --xsct "$PY" --jtag-tcl "$HT/fake_xsct.py")
"${CLIENT[@]}" ping
"$PY" "$HT/test_protocol_errors.py" "$PORT"
"${CLIENT[@]}" infer 0
"${CLIENT[@]}" batch --n "$N_BATCH"
"${CLIENT[@]}" ddr --n "$N_DDR"
echo "host test PASSED"
