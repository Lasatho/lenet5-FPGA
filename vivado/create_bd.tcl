# Block design for the ZYBO (original, xc7z010clg400-1): Zynq PS7 + lenet5_axilite on
# M_AXI_GP0, 50 MHz fabric clock. Builds the bitstream and exports the XSA for Vitis.
#
#   vivado -mode batch -source create_bd.tcl [-tclargs <board_files_dir>]
#
# <board_files_dir> must contain the Digilent "zybo" board files; default is the
# board_files directory of the Vivado install that ships them on this machine.
# Output: lenet5_zybo/ (project), lenet5_zybo.xsa, reports bd_*.rpt next to this script.

set script_dir [file normalize [file dirname [info script]]]
set repo_root  [file normalize [file join $script_dir ..]]
set proj_name  lenet5_zybo
set proj_dir   [file join $script_dir $proj_name]

if {[llength $argv] > 0} {
    set board_repo [lindex $argv 0]
} else {
    set board_repo C:/Xilinx/Vivado/2024.2/data/boards/board_files
}
set_param board.repoPaths [list $board_repo]

create_project $proj_name $proj_dir -part xc7z010clg400-1 -force
set_property board_part digilentinc.com:zybo:part0:1.0 [current_project]
set_property ip_repo_paths [list [file join $repo_root ip_repo]] [current_project]
update_ip_catalog

create_bd_design system

# PS7 with the board preset (DDR, MIO, UART1, PS clock); fabric clock 50 MHz = 20 ns,
# the period lenet5_axilite was scheduled and placed for (Lauf U, wrapper_B)
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7 ps7
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 \
    -config {make_external "FIXED_IO, DDR" apply_board_preset "1" Master "Disable" Slave "Disable"} \
    [get_bd_cells ps7]
set_property -dict [list \
    CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {50} \
    CONFIG.PCW_USE_M_AXI_GP0 {1} \
    CONFIG.PCW_USE_FABRIC_INTERRUPT {1} \
    CONFIG.PCW_IRQ_F2P_INTR {1} \
] [get_bd_cells ps7]

create_bd_cell -type ip -vlnv lasatho:hls:lenet5_axilite:1.0 lenet5
create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset rst_fclk0

# AXI Interconnect with one slave and one master port: no crossbar, only the AXI3 ->
# AXI4-Lite protocol converter. No SmartConnect, the OOC core leaves 120 slices free.
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect axi_ic
set_property -dict [list CONFIG.NUM_SI {1} CONFIG.NUM_MI {1}] [get_bd_cells axi_ic]

connect_bd_intf_net [get_bd_intf_pins ps7/M_AXI_GP0] [get_bd_intf_pins axi_ic/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_ic/M00_AXI] [get_bd_intf_pins lenet5/s_axi_ctrl]

connect_bd_net [get_bd_pins ps7/FCLK_CLK0] \
    [get_bd_pins ps7/M_AXI_GP0_ACLK] \
    [get_bd_pins axi_ic/ACLK] [get_bd_pins axi_ic/S00_ACLK] [get_bd_pins axi_ic/M00_ACLK] \
    [get_bd_pins lenet5/ap_clk] \
    [get_bd_pins rst_fclk0/slowest_sync_clk]
connect_bd_net [get_bd_pins ps7/FCLK_RESET0_N] [get_bd_pins rst_fclk0/ext_reset_in]
connect_bd_net [get_bd_pins rst_fclk0/interconnect_aresetn] [get_bd_pins axi_ic/ARESETN]
connect_bd_net [get_bd_pins rst_fclk0/peripheral_aresetn] \
    [get_bd_pins axi_ic/S00_ARESETN] [get_bd_pins axi_ic/M00_ARESETN] \
    [get_bd_pins lenet5/ap_rst_n]
connect_bd_net [get_bd_pins lenet5/interrupt] [get_bd_pins ps7/IRQ_F2P]

assign_bd_address
validate_bd_design
save_bd_design

set fh [open [file join $script_dir bd_address_map.rpt] w]
foreach seg [get_bd_addr_segs -of_objects [get_bd_addr_spaces ps7/Data]] {
    puts $fh "[get_property NAME $seg] [get_property OFFSET $seg] [get_property RANGE $seg]"
}
close $fh

set bd_file [get_files system.bd]
make_wrapper -files $bd_file -top
add_files -norecurse [file join $proj_dir $proj_name.gen sources_1 bd system hdl system_wrapper.v]
set_property top system_wrapper [current_fileset]

# Same conditions as the OOC runs of the core (templates/vivado_synth.tcl): single-
# threaded synthesis (helper vivado.exe failed to read install files on Windows),
# placement without the utilization pre-check, phys_opt_design after placement.
# The runs are separate Vivado processes, so the parameters go into pre-step hooks.
set hook_synth [file join $script_dir hook_synth_pre.tcl]
set hook_place [file join $script_dir hook_place_pre.tcl]
set fh [open $hook_synth w]; puts $fh "set_param general.maxThreads 1"; close $fh
set fh [open $hook_place w]; puts $fh "set_param place.skipUtilizationCheck 1"; close $fh
set_property STEPS.SYNTH_DESIGN.TCL.PRE $hook_synth [get_runs synth_1]
set_property STEPS.PLACE_DESIGN.TCL.PRE $hook_place [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]

# out-of-context runs of the block design IPs (incl. lenet5_axilite) need the hook too
generate_target all $bd_file
create_ip_run $bd_file
foreach run [get_runs -filter {IS_SYNTHESIS && NAME != synth_1}] {
    set_property STEPS.SYNTH_DESIGN.TCL.PRE $hook_synth $run
}

proc check_run {run} {
    if {[get_property PROGRESS [get_runs $run]] != "100%"} {
        puts "ERROR: run $run failed: [get_property STATUS [get_runs $run]]"
        exit 1
    }
}

launch_runs synth_1 -jobs 1
wait_on_run synth_1
check_run synth_1

launch_runs impl_1 -to_step write_bitstream -jobs 1
wait_on_run impl_1
check_run impl_1

open_run impl_1
report_timing_summary -file [file join $script_dir bd_timing.rpt]
report_utilization -file [file join $script_dir bd_utilization.rpt]
report_utilization -hierarchical -file [file join $script_dir bd_utilization_hier.rpt]

write_hw_platform -fixed -include_bit -force [file join $script_dir $proj_name.xsa]
puts "INFO: XSA written to [file join $script_dir $proj_name.xsa]"
