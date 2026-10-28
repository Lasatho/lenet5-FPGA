set tcldir [file dirname [info script]]
source [file join $tcldir project.tcl]

add_files ${project_name}_prj/solution1/syn/verilog

# Multithreaded synth_design spawns a helper vivado.exe child process; that helper
# failed to read a real, present install file (scripts/rt/data/common.tcl -- verified
# it exists) on this machine, likely a Windows-side process-spawn flakiness (AV
# scanning a freshly spawned exe, handle timing, etc.), not a broken install. Design
# is small enough that single-threaded synthesis costs little time, so just avoid the
# helper-process path entirely instead of chasing the root cause.
set_param general.maxThreads 1

# out_of_context: this RTL is an IP block meant to be instantiated inside a larger
# design (Zynq PS + AXI-Lite wrapper, not built yet) -- without this flag Vivado
# treats every top-level port (4158 of them: full AXI-stream sideband per layer,
# ap_start/done/idle/ready, etc.) as needing a real physical package pin, and
# place_design fails immediately (XC7Z010 clg400 only has 230 usable I/O).
synth_design -top ${project_name} -part $part -mode out_of_context

# out-of-context module synth has no constraints file; without a clock, place/route
# have nothing to optimize timing against and report_timing_summary is meaningless.
create_clock -period $clock_period -name ap_clk [get_ports ap_clk]

opt_design -retarget -propconst -sweep -bram_power_opt -shift_register_opt
report_utilization -file vivado_synth.rpt
report_utilization -hierarchical -file vivado_synth_hier.rpt

# Diagnostic (first attempt, io_stream/Lauf F): place_design failed with "205 control
# sets" pushing required slices past the device's total physical slice count -- raw
# LUT/FF counts (from report_utilization above) don't capture this, since a slice can
# only pack FFs sharing the same clock-enable/reset combination. report_control_sets
# itself named the fix: "Control sets can be merged at opt_design using
# control_set_merge or merge_equivalent_drivers".
report_control_sets -verbose -file vivado_control_sets_before.rpt

# Second opt_design pass with the two switches report_control_sets pointed at:
# -control_set_merge groups FFs whose control sets differ only cosmetically,
# -merge_equivalent_drivers collapses logically-identical enable/reset nets that
# synthesis emitted as textually distinct signals (common in HLS-generated RTL, where
# the same condition gets recomputed locally in multiple places instead of shared).
opt_design -control_set_merge -merge_equivalent_drivers
report_control_sets -verbose -file vivado_control_sets_after.rpt
report_utilization -file vivado_synth_after_control_set_merge.rpt

# Vivado's own automated merge/retarget suggestions -- checked once already (io_stream
# run, Lauf H): only gave generic global utilization warnings, no netlist-specific
# suggestions (those need a successfully routed design as a baseline, which we never
# reached). Kept in the script anyway since it's a free extra report.
report_qor_suggestions -file vivado_qor_suggestions.rpt

# Vivado refuses to even attempt placement above its internal utilization heuristic.
# Force a real attempt instead of trusting the heuristic.
set_param place.skipUtilizationCheck 1
place_design
phys_opt_design
report_utilization -file vivado_impl.rpt

route_design
report_utilization -file vivado_route.rpt
report_timing_summary -file vivado_timing_route.rpt
report_route_status -file vivado_route_status.rpt
