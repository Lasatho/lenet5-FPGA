# LeNet-5 auf ZYBO (Zynq Z-7010)

Deployment von LeNet-5 (MNIST-Klassifikation) als IP-Core via hls4ml auf dem Digilent
ZYBO, Xilinx Zynq XC7Z010-1CLG400C.

## Stack
- Training: PyTorch (`model/`)
- HLS-Synthese: hls4ml 1.3.0 + Vitis HLS 2024.2
- Hardware: Vivado Block Design (AXI-Lite)
- Software: Vitis Bare-Metal C
- PC-Client: Python / pyserial

## Struktur
- `model/lenet5.py`, `train.py` – Modell und Training
- `model/finetuning.py` – Optuna-Suche für structured Pruning
- `model/rebuild_trial7.py` – Nachbau des gewählten Modells nach `model/pruned_best.pth`
- `model/precision_sweep.py` – bitgenauer Precision-Sweep in hls4ml (ohne Vitis)
- `model/export.py` – Export nach `hls4ml/lenet5_project/`
- `hls4ml/templates/` – versionierte `build_opt.tcl`/`vivado_synth.tcl`, von `export.py` ins Projekt kopiert
- `hls4ml/wrapper/` – AXI-Lite-Wrapper `lenet5_axilite` um den Kern (Bild, Logits und
  Steuerung über ein `s_axilite`-Interface), Testbench und `build_wrapper.tcl`
- `ip_repo/lenet5_mnist_1_0/` – exportierter Kern `lasatho:hls:lenet5_mnist:1.0`
  (AXI-Stream ohne `TLAST`, `ap_ctrl_hs`); in Vivado als IP-Repository `ip_repo/` eintragen
- `ip_repo/lenet5_axilite_1_0/` – AXI-Lite-Wrapper `lasatho:hls:lenet5_axilite:1.0` samt
  Bare-Metal-Treiber (`drivers/`), Adresskarte in `xlenet5_axilite_hw.h`
- `vivado/create_bd.tcl` – Block Design (PS7 + `lenet5_axilite` an M_AXI_GP0, 50 MHz),
  Bitstream und XSA für das ZYBO
- `vitis/create_platform.py` – Vitis-Plattform `lenet5_zybo_pf` aus der XSA (Standalone auf
  `ps7_cortexa9_0`, FSBL, stdin/stdout auf UART1)
- `vitis/create_app.py`, `vitis/app/src/` – Bare-Metal-App `lenet5_app` (UART-Protokoll in
  `protocol.h`, Inferenz per UART oder über den Testsatz im DDR)
- `vitis/jtag.tcl` – `xsct`-Hilfsskript: Board starten (`boot`), Testsatz ins DDR laden
  (`load`), Ergebnisse zurücklesen (`read`)
- `vitis/app/host_test/` – Test der App und des PC-Clients ohne Board: `main.c` mit Stubs
  auf dem Host, C-Modell als IP, Pseudo-Terminal als UART
- `pc_client/prepare_dataset.py` – Vorverarbeitung des MNIST-Testsatzes auf dem PC (Resize
  32×32, Normalisierung, `ap_fixed<11,5>`-Rohwörter als `int16`) nach `pc_client/data/`
- `pc_client/c_reference.cpp` – bitgenaue Referenz für den Hardwaretest: prüft die
  Quantisierung gegen `ap_fixed`, rechnet alle Bilder durch das C-Modell `myproject()`
- `pc_client/send_image.py` – PC-Client (`ping`, `infer`, `batch`, `ddr`), vergleicht die
  Logits bitgenau mit `ref_logits.bin`

## Ablauf
```
pip install -r requirements.txt
python model/export.py          # aus dem Repo-Root aufrufen
```
`export.py` patcht `nnet_dense_stream.h` (zyklisch partitionierter Dense-Eingangspuffer)
und `build_prj.tcl` (`-complete_threshold`, Pfade der C/RTL-Validierung) und schreibt
100 MNIST-Testvektoren nach `tb_data/`. `build_opt.tcl` und `vivado_synth.tcl` (OOC,
Placement/Routing, Reports) kopiert es aus `hls4ml/templates/`; Änderungen an diesen
Skripten dort vornehmen.

Vitis HLS/Vivado laufen unter Windows; das Projekt wird dafür nach
`C:\Users\lasse\vitis_work\lenet5_project` gespiegelt und dort `build_prj.tcl`
per PowerShell ausgeführt.

AXI-Lite-Wrapper (setzt das generierte `hls4ml/lenet5_project/` voraus): `hls4ml/` mit
`lenet5_project/`, `wrapper/` und `templates/` spiegeln, im Ordner `wrapper/`
`vitis_hls -f build_wrapper.tcl` ausführen; Schritte über `opt` am Skriptanfang. Die
Testbench vergleicht je Bild bitgenau mit `myproject()` und mit `tb_output_predictions.dat`.
Logits stehen vorzeichenerweitert als 32-Bit-Rohwert im Register (Wert = Wort / 2^12).

Vitis-Plattform (Vitis 2024.2, unter Windows): `vitis -s vitis/create_platform.py
<lenet5_zybo.xsa> [<workspace>]`; der Workspace (Standard `vitis/workspace`) wird bei
jedem Lauf neu angelegt, die Plattform liegt danach unter
`lenet5_zybo_pf/export/lenet5_zybo_pf/lenet5_zybo_pf.xpfm`.

Testdaten für die Hardware (aus dem Repo-Root, setzt `hls4ml/lenet5_project/` voraus):
```
python pc_client/prepare_dataset.py
g++ -O2 -std=c++14 -I hls4ml/lenet5_project/firmware -I hls4ml/lenet5_project/firmware/ap_types \
    pc_client/c_reference.cpp hls4ml/lenet5_project/firmware/myproject.cpp -o pc_client/data/c_reference
pc_client/data/c_reference
```
`c_reference` schreibt die erwarteten Logits (`ref_logits.bin`, dieselben 32-Bit-Wörter wie
der Wrapper) und bricht ab, wenn die Quantisierung oder die ersten Bilder nicht bitgleich zu
`tb_output_predictions.dat` sind.

Bare-Metal-App (unter Windows, nach der Plattform; zwischen zwei `vitis -s`-Läufen auf
demselben Workspace warten, bis der Vitis-Server beendet ist, sonst „workspace is already
in use“): `vitis -s vitis/create_app.py <xpfm> [<workspace>] [<baud>]`. Hardwaretest:
```
xsct vitis/jtag.tcl boot <lenet5_zybo.bit> <ps7_init.tcl> <lenet5_app.elf>
python pc_client/send_image.py --port COMx ping|infer <i>|batch [--n N]|ddr [--n N]
```
Bitstream und `ps7_init.tcl` liegen im Plattform-Export unter `hw/sdt/`. Ohne Board:
`vitis/app/host_test/run_host_test.sh` (aus dem Repo-Root, nach den Testdaten).
