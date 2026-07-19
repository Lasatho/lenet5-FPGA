# LeNet-5 auf ZYBO (Zynq Z-7010)

Deployment von LeNet-5 (MNIST-Klassifikation) auf dem Digilent ZYBO Board via hls4ml.

## Stack
- Training: PyTorch / Keras
- HLS-Synthese: hls4ml + Vitis HLS
- Hardware: Vivado Block Design (AXI-Lite)
- Software: Vitis Bare-Metal C
- PC-Client: Python / pyserial

## Board
Digilent ZYBO, Xilinx Zynq XC7Z010-1CLG400C

## Status (2026-07-19)
- **Training/Export**: fertig. Keras-Modell trainiert, ONNX-Export vorhanden (`model/`).
- **hls4ml-Projekt**: generiert (`hls4ml/lenet5_project/`), Backend Vitis, Part `xc7z010clg400-1`, `ap_fixed<16,6>`, `IOType: io_parallel`, `Strategy: Latency`, `ReuseFactor: 64`.
- **HLS-Synthese (csynth)**: noch nicht erfolgreich durchgelaufen. Vorheriger Versuch nach 7 Tagen abgebrochen (Design vermutlich zu groß/parallel für XC7Z010 — siehe Hinweis unten). Projekt wurde danach zurückgesetzt und neu generiert (`reset`, `regenerate fresh project`).
- **Offener Punkt vor nächstem Lauf**: `Strategy: Latency` ignoriert `ReuseFactor` weitgehend (nur unter `Strategy: Resource` wird zeitgemultiplext) → Design läuft praktisch voll parallel, was auf dem kleinsten Zynq (80 DSPs) zu exzessiver Synthesezeit/Ressourcenbedarf führt. Vor dem nächsten Mehrtages-Lauf sollte das geklärt werden.
- **Vivado Block Design (AXI-Lite)**: noch nicht begonnen (`vivado/` leer).
- **Vitis Bare-Metal Software**: noch nicht begonnen (`vitis/` leer).
- **PC-Client**: Grundgerüst nur als leere Datei (`pc_client/send_image.py`).
