# Vitis platform for the ZYBO block design (vivado/create_bd.tcl): standalone domain on
# ps7_cortexa9_0 plus the Zynq boot domain (FSBL), built from the exported XSA.
#
#   vitis -s create_platform.py <xsa> [<workspace>]
#
# <workspace> defaults to ./workspace next to this script and is recreated on every run.
# Output: <workspace>/lenet5_zybo_pf/export/lenet5_zybo_pf/lenet5_zybo_pf.xpfm

import os
import shutil
import sys

import vitis

PLATFORM = "lenet5_zybo_pf"
DOMAIN = "standalone_ps7_cortexa9_0"

xsa = os.path.abspath(sys.argv[1])
script_dir = os.path.dirname(os.path.abspath(__file__))
workspace = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(script_dir, "workspace")

if os.path.isdir(workspace):
    shutil.rmtree(workspace)

client = vitis.create_client()
client.set_workspace(path=workspace)

platform = client.create_platform_component(
    name=PLATFORM, hw_design=xsa, os="standalone", cpu="ps7_cortexa9_0", domain_name=DOMAIN)

# ZYBO: USB-UART is UART1 (MIO 48/49)
domain = platform.get_domain(DOMAIN)
domain.set_config("os", "standalone_stdin", "ps7_uart_1")
domain.set_config("os", "standalone_stdout", "ps7_uart_1")

platform.build()
platform.report()
domain.report()

vitis.dispose()
