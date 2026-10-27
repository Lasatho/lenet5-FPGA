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
- `vivado/`, `vitis/`, `pc_client/` – noch leer

## Ablauf
```
pip install -r requirements.txt
python model/export.py          # aus dem Repo-Root aufrufen
```
`export.py` patcht `nnet_dense_stream.h` (zyklisch partitionierter Dense-Eingangspuffer)
und schreibt 100 MNIST-Testvektoren nach `tb_data/`. Danach von Hand:
- `build_opt.tcl`: `cosim 0`, `validation 0`, `vsynth 1`
- `build_prj.tcl`: `config_array_partition -maximum_size` → `-complete_threshold`
- `vivado_synth.tcl`: versionierte Fassung (OOC, Placement/Routing, Reports)
  wiederherstellen, z. B. `git checkout hls4ml/lenet5_project/vivado_synth.tcl`

Vitis HLS/Vivado laufen unter Windows; das Projekt wird dafür nach
`C:\Users\lasse\vitis_work\lenet5_project` gespiegelt und dort `build_prj.tcl`
per PowerShell ausgeführt.
