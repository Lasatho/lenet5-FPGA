# Builds the bare-metal app (vitis/app/src) against the platform from create_platform.py.
#
#   vitis -s create_app.py <xpfm> [<workspace>] [<baud>]
#
# <workspace> defaults to ./workspace next to this script (the platform workspace can be
# reused); the app component is recreated on every run. <baud> sets UART_BAUD
# (default 921600 in main.c; 115200 for bring-up with a plain terminal).
# Output: <workspace>/lenet5_app/build/lenet5_app.elf

import os
import sys

import vitis

APP = "lenet5_app"
DOMAIN = "standalone_ps7_cortexa9_0"
SOURCES = ["main.c", "protocol.h"]

xpfm = os.path.abspath(sys.argv[1])
script_dir = os.path.dirname(os.path.abspath(__file__))
workspace = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(script_dir, "workspace")
baud = sys.argv[3] if len(sys.argv) > 3 else None

client = vitis.create_client()
client.set_workspace(path=workspace)
# a workspace other than the platform's only finds the platform via its export directory
client.add_platform_repos(os.path.dirname(os.path.dirname(xpfm)))

if os.path.isdir(os.path.join(workspace, APP)):
    client.delete_component(name=APP)

app = client.create_app_component(name=APP, platform=xpfm, domain=DOMAIN, template="empty_application")
app.import_files(from_loc=os.path.join(script_dir, "app", "src"), files=SOURCES, dest_dir_in_cmp="src")
if baud:
    app.append_app_config(key="USER_COMPILE_DEFINITIONS", values=[f"UART_BAUD={baud}"])
app.build()
app.report()
vitis.dispose()

# build() does not raise on compile errors
elf = os.path.join(workspace, APP, "build", APP + ".elf")
if not os.path.isfile(elf):
    print(f"ERROR: {elf} not built")
    sys.exit(1)
print(f"INFO: {elf}")
