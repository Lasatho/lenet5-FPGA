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
- `vivado/`, `vitis/`, `pc_client/` – noch leer

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
