# JTAG helper for the ZYBO (xsct from Vitis 2024.2, hw_server on the local machine).
#
#   xsct jtag.tcl boot <bit> <ps7_init.tcl> <elf>   program PL, init PS, download and run app
#   xsct jtag.tcl load <images.bin> <labels.bin>   DDR mode: test set into DDR
#   xsct jtag.tcl read <results.bin> <n>           DDR mode: read back n results
#
# <bit> and <ps7_init.tcl> are in the platform export (hw/sdt/), <elf> from create_app.py.
# Addresses and the result size must match vitis/app/src/protocol.h.

set DDR_IMAGE_BASE  0x10000000
set DDR_LABEL_BASE  0x11400000
set DDR_RESULT_BASE 0x11500000
set RESULT_BYTES    48

proc select_a9 {} {
    targets -set -nocase -filter {name =~ "*A9*#0"}
}

proc boot {bit ps7_init elf} {
    targets -set -nocase -filter {name =~ "*APU*"}
    rst -system
    after 1000
    fpga -file $bit
    targets -set -nocase -filter {name =~ "*APU*"}
    source $ps7_init
    ps7_init
    ps7_post_config
    select_a9
    rst -processor
    dow $elf
    con
}

# the app waits for UART input meanwhile; stop/con around the memory access
proc load {images labels} {
    select_a9
    stop
    dow -data $images $::DDR_IMAGE_BASE
    dow -data $labels $::DDR_LABEL_BASE
    con
}

proc read_results {out n} {
    select_a9
    stop
    mrd -bin -file $out $::DDR_RESULT_BASE [expr {$n * $::RESULT_BYTES / 4}]
    con
}

connect
set cmd [lindex $argv 0]
switch -- $cmd {
    boot    { boot {*}[lrange $argv 1 3] }
    load    { load {*}[lrange $argv 1 2] }
    read    { read_results {*}[lrange $argv 1 2] }
    default { puts "usage: xsct jtag.tcl boot|load|read ..."; exit 1 }
}
disconnect
