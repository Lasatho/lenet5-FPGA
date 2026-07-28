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

## Status (2026-07-22)
- **Training/Export**: fertig. Keras-Modell trainiert, ONNX-Export vorhanden (`model/`).
- **hls4ml-Projekt**: generiert (`hls4ml/lenet5_project/`), Backend Vitis, Part `xc7z010clg400-1`, `ap_fixed<16,6>`, `IOType: io_stream`, `Strategy: Resource`, `BramFactor: 100`, `ClockPeriod: 14` (ns). `ReuseFactor` **pro Layer** (`granularity='name'` in `export.py`) statt global.
- **Bugfix `build_prj.tcl`** (hls4ml-Template-Bug, betrifft jede generierte Vitis-Projekt-Instanz): `catch {config_array_partition -maximum_size 4096}` nutzt eine für Vitis HLS 2024.2 veraltete Option und schlug in *jedem* bisherigen Lauf lautlos fehl (`catch` schluckt den Fehler) — der Guard war nie aktiv. Korrigiert auf `-complete_threshold 4096` (aktuelle Syntax). Muss nach jedem `export.py`-Regenerieren erneut gepatcht werden, da hls4ml das Template jedes Mal neu schreibt (gleiches Problem wie bei `cosim`/`validation` in `build_prj.tcl`).
- **Bugfix `model/export.py`**: ONNX-Export-Subprozess nutzte System-`python3` statt venv-Interpreter (`sys.executable`) — `tf2onnx` war dadurch nicht auffindbar.
- **HLS-Synthese (csynth) — Iterationsverlauf** (`myproject_csynth.rpt`, gegen XC7Z010: 80 DSP / 17.600 LUT / 35.200 FF / 120 BRAM_18K):

  | Lauf | Config | Zeit | DSP | BRAM | FF | LUT |
  |---|---|---|---|---|---|---|
  | 0 (vor dieser Session) | `io_parallel`, `Strategy: Latency`, RF=64 | 7 Tage, abgebrochen | — | — | — | — |
  | 1 | `io_parallel`, `Strategy: Resource`, RF=1500 (global) | ~13h, abgebrochen (24,8GB RAM) | — | — | — | — |
  | 2 | `io_stream`, RF=1500 (global), Takt 5ns | 27 min ✓ | 42 (52%) ✓ | 74 (61%) ✓ | 53.695 (152%) ✗ | 41.068 (233%) ✗ |
  | 3 | `io_stream`, RF=8000 (global), Takt 14ns | 8m26s ✓ | 11 (13%) ✓ | 83 (69%) ✓ | 41.241 (117%) ✗ | 45.845 (260%) ✗ |
  | 4 | `io_stream`, RF **pro Layer**, Takt 14ns | ✓ | 17 (21%) ✓ | 77 (64%) ✓ | 42.039 (119%) ✗ | 41.844 (237%) ✗ |
  | 5 | wie 4 + RF auf Dense-ReLUs erzwungen | ✓ | 17 (21%) ✓ | 77 (64%) ✓ | 42.039 (119%) ✗ | 41.844 (237%) ✗ (identisch zu Lauf 4) |
  | 6 | wie 4 + `BramFactor` 1000→100 + `config_array_partition`-Fix | ✓ | 17 (21%) ✓ | 75 (62%) ✓ | 42.044 (119%) ✗ | 41.848 (237%) ✗ (identisch zu Lauf 4/5) |
  | 7 | wie 6 + `vsynth=1` (echte Vivado-Synthese statt HLS-Schätzung) | ✓ | — | — | — | — |
  | 8 | wie 7 + `opt_design` | ✓ | — | — | — | — |
  | 9 | wie 8 + Dense1 (`dense`) RF 8000→48000 | abgebrochen (18,6GB RAM) | — | — | — | — |
  | 10 | wie 8, Dense1 zurück auf RF=8000 (finaler Stand dieses Tuning-Asts) | — | — | — | — | — |

  **Lauf 1→2:** `io_parallel` partitioniert alle Zwischen-Feature-Maps komplett in Register statt sie als FIFOs zu streamen — Ursache der RAM-/Zeitexplosion. `io_stream` behebt das (Zwischenschichten als `hls::stream`, DATAFLOW dabei kanonisch korrekt statt der vorherigen fehlerhaften Form).
  **Lauf 2→3:** DSP/FF deutlich verbessert (globaler RF-Anstieg, v.a. Dense1 400→120 ging von 30 auf 6 DSPs), aber LUT wurde *schlechter* (233%→260%). Zwei Variablen gleichzeitig geändert (RF und ClockPeriod 5→14ns) — Konfundierung, siehe Future-Work.
  **Lauf 3→4:** Ursache für den LUT-Anstieg eingegrenzt auf einzelne Layer: Dense2 (120→84, `config11`) wurde bei voller Serialisierung (RF 1440→10080) trotz DSP-Ersparnis (7→1) beim LUT schlechter (6.153→10.182) — Kontroll-/Adresslogik-Overhead überwiegt den DSP-Gewinn, den wir eh nicht brauchen (DSP-Budget bei 13% Auslastung). Deshalb **pro Layer** eingestellt statt global: `conv2d`=150, `conv2d_1`=2400, `dense` (Dense1)=8000 (beibehalten, hat geholfen), `dense_1` (Dense2)=1440 (zurückgesetzt), `dense_2`=840. `ClockPeriod` konstant bei 14ns gehalten für sauberen Vergleich — bestätigt: Dense2-LUT ging auf 6.181 zurück, Gesamt-LUT 260%→237%. DSP/BRAM weiter komfortabel (21%/64%), LUT bleibt Flaschenhals (2,37x über Budget), FF knapp drüber (1,19x).
  **Lauf 4→5:** Hypothese war: `relu_config10`/`relu_config12` (0 Zyklen Latenz, voll kombinatorisch, 4.589/3.230 LUT) ließen sich durch `ReuseFactor` auf den Activation-Layern selbst (`dense_relu`=120, `dense_1_relu`=84) zu elementweisem Looping zwingen. **Ergebnis: exakt identischer Report, kein Effekt.** Grund (im Nachhinein klar): Dense-Layer (Resource-Strategy) liefert sein Ergebnis nicht elementweise gestreamt, sondern als einen einzigen breiten Transfer (FIFO-Tiefe=1 bestätigt im Report) — die komplette Ausgabe ist erst nach dem vollen RF-Rechenloop fertig, es gibt keinen Stream zum drüber-loopen. ReuseFactor der ReLU greift hier prinzipbedingt nicht. Sackgasse, Änderung wieder aus `export.py` entfernt.
  **Loop Constraint Status "NOT satisfied"** (Meldung in jedem Lauf): Vitis-HLS-Log zeigt konkret: Dense1/Dense2 `ReuseLoop` erreicht nur Final II=3/4 statt Ziel II=1, weil die Gewichts-BRAMs nur 2 Leseports haben (*"limited memory ports"*). Reine Latenz-Qualitätsmeldung, kein Fehler — der vorgeschlagene Fix (mehr Ports/Partitionierung) würde zusätzliche Ressourcen kosten, also genau gegenteilig zu unserem Ziel. Bewusst ignoriert.
  **Lauf 5→6:** Zwei unabhängige Fixes. (1) `BramFactor` 1000→100: Conv1-Gewichte (150 Elemente) und Dense3-Gewichte (840 Elemente) lagen unter der alten Schwelle und damit als Register/LUT statt BRAM markiert — BRAM hat mit 64% Auslastung noch Luft. (2) `config_array_partition -maximum_size 4096` in `build_prj.tcl` ist eine für Vitis HLS 2024.2 veraltete Option (stammt unverändert aus dem hls4ml-Paket-Template) und lief in *jedem* bisherigen Lauf lautlos ins Leere (`catch` verschluckt den Fehler) — korrigiert auf `-complete_threshold 4096`, war vorher nie aktiv. **Ergebnis: wieder praktisch kein Effekt** (BRAM sogar leicht runter, 77→75; LUT/FF unverändert). Vitis HLS setzt so winzige Arrays (150/840 Elemente) trotz `storage=bram`-Hinweis nicht als echte BRAM-Primitive um — ein ganzer 18Kb-Block lohnt sich dafür nicht, Tool bleibt bei Registern.
  **Lauf 6→7:** Mehrere "billige" Stellschrauben (RF-Verteilung, BramFactor, Array-Partition-Threshold) haben LUT nicht mehr bewegt, hängt seit Lauf 4 bei ~237%. Der `myproject_csynth.rpt` ist aber nur eine **Vor-Synthese-Schätzung** von Vitis HLS, kein echtes Vivado-Ergebnis. `vsynth=1` gesetzt, lässt eine echte Out-of-Context-Vivado-Synthese auf dem exportierten RTL laufen (`vivado_synth.rpt`) statt der HLS-Schätzung zu vertrauen.

  **Lauf 7 — echte Vivado-Synthese statt HLS-Schätzung** (`vivado_synth.rpt`, `synth_design` Out-of-Context, gegen XC7Z010: 80 DSP / 17.600 LUT / 35.200 FF / 60 BRAM-Tiles):

  | Resource | HLS-Schätzung (Lauf 6) | Echte Vivado-Synthese | Util % |
  |---|---|---|---|
  | DSP | 17 | **12** | 15% ✓ |
  | BRAM | 75/120 (18K-Blöcke) | **24,5 Tiles** (18×RAMB36 + 13×RAMB18) | 41% ✓ |
  | FF | 42.044 | **43.840** | 124,6% ✗ |
  | LUT | 41.848 | **24.466** | 139,0% ✗ |

  HLS-Schätzung war für LUT deutlich zu pessimistisch (237%→139% real), für FF ungefähr richtig. Hinweis im Report ignoriert: `Bonded IOB 2748/100 = 2748%` — Artefakt der Out-of-Context-Synthese (isoliertes Modul, jeder Port zählt als Pin), kein echtes Problem im fertigen Block Design.

  **Lauf 8 — `opt_design`** (Vivado-eigener Hinweis: "Final LUT count ... typically lower, run opt_design"): kaum Bewegung — LUT 139,0%→136,4%, FF 124,6%→124,4%. An der Grenze dessen, was reine Logikoptimierung ohne Place&Route noch bringt.

  **Lauf 9 — Dense1 volle Serialisierung (RF 8000→48000, 1 statt 6 DSP):** nach Nutzerfreigabe als letzter Tuning-Versuch vor Quantisierung. Scheduling der `ReuseLoop` hing >15min, RAM auf 18,6GB gestiegen — gleiches Explosionsmuster wie bei Dense2 vorher (volle Serialisierung erzeugt mehr Kontroll-/Adresslogik-Overhead als sie an DSPs spart), nur diesmal an der größten Schleife im Design. Abgebrochen.

  **Lauf 10 — Tuning-Ast abgeschlossen:** Dense1 zurück auf RF=8000 (letzter bekannt guter Stand, Lauf 8: DSP 15%, BRAM 41%, FF 124,4%, LUT 136,4%). Weiteres RF-/BramFactor-/Array-Partition-Feintuning ausgereizt, keine der "billigen" Stellschrauben bewegt LUT/FF noch nennenswert. **Fazit: Ohne Precision-Änderung passt das Design nicht aufs XC7Z010** (LUT +36%, FF +24% über Budget). Nächster Schritt laut Entscheidung: Quantisierung/Pruning zunächst softwareseitig (PyTorch/QKeras) untersuchen, bevor wieder an der HLS-Synthese weitergearbeitet wird — HLS-Tuning hier pausiert.
- **Ausführung läuft nativ unter Windows**, nicht in WSL: die WSL-seitige Vitis-HLS-Installation hat keine Linux-Binaries (`lnx64.o` fehlt), nur die Windows-Installation unter `C:\Xilinx-01\Vitis_HLS\2024.2` funktioniert. Projekt wird dafür nach `C:\Users\lasse\vitis_work\lenet5_project` gespiegelt (vermeidet zusätzlich langsamen `\\wsl$`-Netzwerk-I/O) und dort per PowerShell gestartet.
- **Vivado Block Design (AXI-Lite)**: noch nicht begonnen (`vivado/` leer).
- **Vitis Bare-Metal Software**: noch nicht begonnen (`vitis/` leer).
- **PC-Client**: Grundgerüst nur als leere Datei (`pc_client/send_image.py`).

## Quantisierung & Pruning (Untersuchung, in Arbeit)
Grund: HLS-Tuning (RF/Strategy/BramFactor) ist ausgereizt, Design passt bei `ap_fixed<16,6>` nicht aufs XC7Z010 (LUT +36%, FF +24% über Budget, siehe Lauf 10 oben). Untersuchung läuft aktuell softwareseitig, getrennt vom HLS-Projekt.

- **Ziel**: Genauigkeitsverlust durch Quantisierung (niedrigere Bitbreite als `ap_fixed<16,6>`) und ggf. Pruning quantifizieren, bevor erneut synthetisiert wird.
- **Werkzeuge**: PyTorch/QKeras (quantization-aware training bevorzugt gegenüber reinem Post-Training-Quantization, da LeNet5 klein genug für günstiges Nachtrainieren ist).
- **Zu erfassende Metriken**: Test-Accuracy je Bitbreite/Pruning-Rate, Vergleich gegen aktuelle `ap_fixed<16,6>`-Baseline.

**Ergebnis structured Pruning (2026-07-24, `model/finetuning.py`):** Optuna Multi-Objective-Suche (maximiere Accuracy, minimiere Parameterzahl) über `ratio_fc`/`ratio_fc1`/`iterative_steps`/`fine_tune_epochs`/`fine_tune_lr`, 20 Trials, `torch_pruning` `GroupMagnitudeImportance` (magnitude-basiertes Channel-Pruning, Weg B) auf `fc` (400→120) und `fc1` (120→84); `fc2` nicht direkt geprunt, aber strukturell mitgekoppelt. Baseline: 61,8K Parameter.

Pareto-Front (Auswahl):

| Trial | ratio_fc | ratio_fc1 | Acc | Params | Grobe LUT/FF-Schätzung* |
|---|---|---|---|---|---|
| 9 | 0.9 | 0.7 | 98,71% | 7,6K | LUT ~17%, FF ~15% |
| 5 | 0.8 | 0.6 | 99,20% | 13,0K | LUT ~29%, FF ~26% |
| **7** | **0.7** | **0.7** | **99,34%** | **18,2K** | **LUT ~40%, FF ~37%** |
| 8 | 0.3 | 0.7 | 99,35% | 38,7K | LUT ~86%, FF ~78% |

*Grobe lineare Skalierung auf Basis Lauf-10-Referenz (61,47K Params → LUT 136%, FF 124% über Budget) — Annahme, nicht durch echte Synthese verifiziert, Fixkosten-Anteil unbekannt.

**Trial 7 ist der Favorit** (bester Kompromiss aus Genauigkeit und Größe, komfortabler Ressourcenpuffer geschätzt). Das Skript wählt automatisch per `max(Accuracy)` unter der Pareto-Front aus (`finetuning.py:143`) und hatte dadurch ursprünglich **Trial 8** gespeichert (99,31% bei Re-Run, 38,7K Params) — für +0,01% Accuracy über 2x so groß wie Trial 7.

**Update (2026-07-28):** Trial-7-Konfiguration (`ratio_fc=0.7, ratio_fc1=0.7, iterative_steps=4, fine_tune_epochs=4, fine_tune_lr=0.000232`) über eigenständiges Skript (`model/rebuild_trial7.py`, dupliziert `prune_and_evaluate` statt `finetuning.py` zu importieren — sonst würde der komplette 20-Trial-Optuna-Lauf erneut anlaufen) nachgebaut und als neues `model/pruned_best.pth` gespeichert (überschreibt Trial 8). Resultierende Architektur: `fc` 400→36, `fc1` 36→25, `fc2` 25→10 (Layer-Shapes aus echtem hls4ml-Parse-Lauf bestätigt, s.u.).

Nebenbefund: `finetuning.py` lief ohne `python -u`/`PYTHONUNBUFFERED=1` — eigene `print()`-Ausgaben blieben im stdout-Buffer hängen, nur Optunas interne Logger-Zeilen kamen sofort durch. Für künftige Läufe `-u` verwenden.

**Hilft Pruning der Synthese?** Kommt auf die Art an:
- **Strukturiertes Pruning** (ganze Neuronen/Filter/Kanäle entfernen, z.B. Dense1 400→120 auf 400→64 kürzen oder Conv2-Filter 16→weniger) hilft direkt und zuverlässig — reduziert `n_in`/`n_out`/Filterzahl der Layer selbst, wirkt sich automatisch auf **alle** Ressourcen aus (DSP, LUT, FF, BRAM), unabhängig von hls4ml-Konfiguration. Für unser Problem (LUT/FF-Überschreitung bei Dense1 als Haupttreiber) der direktere Hebel.
- **Unstrukturiertes Pruning** (einzelne Gewichte auf 0 setzen, Sparsity im Gewichts-Array) bringt **ohne Weiteres gar nichts** — hls4mls Standard-Implementierung behandelt alle Gewichte als dicht besetzt und instanziiert dieselbe MAC-Hardware unabhängig davon, ob ein Wert 0 ist. Nur mit hls4mls `Compression`-Option (komprimierte Gewichtsdarstellung, überspringt bekannte Nullen zur Synthesezeit) wirkt sich Sparsity tatsächlich auf DSP-Zahl und Reuse-Loop-Länge aus — ohne das Flag ist ein unstrukturiert gepruntes Modell hardwareseitig identisch zum ungeprunten.
- Nach Pruning ist Nachtrainieren (Fine-Tuning) nötig, um die Genauigkeit zurückzugewinnen — Kombination Pruning+Quantisierung (erst pruning, dann quantization-aware fine-tuning) ist der übliche Ansatz.

## Export-Pipeline: Segfault-Fix, hls4ml-Upgrade, RF-Neuberechnung (2026-07-28)

Nach dem Trial-7-Rebuild schlug `model/export.py` mit `Segmentation fault (core dumped)` fehl, ohne Python-Traceback. Ursachenkette und Fixes:

- **Segfault-Ursache**: `model/pruned_best.pth` wird per `torch.save(model, ...)` als volles Objekt gespeichert (nicht nur `state_dict`). Das Laden per `torch.load(..., weights_only=False)` unpickelt die `LeNet5`-Klasse und zieht dabei `torchvision` (inkl. nativer Op-Registrierung, z.B. `roi_align`) nach. War zu diesem Zeitpunkt bereits TensorFlow/oneDNN im selben Prozess geladen (z.B. durch vorherigen `import hls4ml`), kollidieren die nativen Bibliotheken beim Registrieren → SIGSEGV. **Fix**: `torch.load(...)` muss vor `import hls4ml` passieren, nicht danach. Faustregel für dieses Projekt: PyTorch-Objekt-Laden immer als aller-erste Aktion im Skript, TensorFlow-abhängige Imports (hls4ml, keras) danach.
- **hls4ml-Versionsproblem**: installierte Version war unversioniert (kein `requirements.txt`) bei 0.7.1. `hls4ml.utils.config_from_pytorch_model()` ist dort (und in 0.8.1) ein **Stub** — der `granularity`-Parameter wird trotz Docstring komplett ignoriert, es wird nie ein `'LayerName'`-Key erzeugt. Erst **hls4ml 1.3.0** hat eine echte PyTorch-Parser-Implementierung für `granularity='name'`. Jetzt in neu angelegter `requirements.txt` gepinnt (`hls4ml==1.3.0`). API-Änderungen durch den Versionssprung: `input_shape` wandert von `convert_from_pytorch_model()` zu `config_from_pytorch_model()` (dort ohne Batch-Dimension, z.B. `(1,32,32)` statt `(1,1,32,32)`), `part`-Argument entfällt bei `convert_from_pytorch_model()` (kommt aus der Config).
- **`forward()`-Fix in `model/lenet5.py`**: `out.reshape(out.size(0), -1)` → `torch.flatten(out, 1)`. hls4mls FX-basierter Tracer (1.3.0) kennt `Tensor.size()` nicht (`Unsupported function size`). Funktional identisch, kein Neu-Training nötig — der Pickle speichert nur Parameter/Zustand, `forward` kommt beim Laden frisch aus der Modul-Datei.
- **Layer-Namen geändert**: mit hls4ml 1.3.0 heißen die geparsten Layer nach den echten PyTorch-Attributnamen (`layer1_0`, `layer2_0`, `fc`, `fc1`, `fc2`), nicht mehr nach Keras-Konvention (`conv2d`, `dense`, ...). `export.py` entsprechend angepasst.
- **ReuseFactor neu berechnet** für die durch Pruning verkleinerten Dense-Layer (`fc` 400→36, `fc1` 36→25, `fc2` 25→10). hls4mls Resource-Strategy-Dense verlangt `RF ∈ Teiler(n_in) ∪ {n_in·d : d teilt n_out}` (bestätigt per `nnet_dense_resource.h`-Verhalten: ungültige RF-Werte werden von hls4ml automatisch auf den nächstgültigen Wert *nach unten* korrigiert, mit Warnung). Conv-Layer sind vom Pruning nicht betroffen, RF unverändert (150/2400, weiterhin voll serialisiert, 1 DSP je). Für die Dense-Layer bewusst **niedrigere** RF gewählt als reine proportionale Skalierung nahelegen würde: DSP-Budget hatte vor dem Pruning nur 15% Auslastung (12/80), der tatsächliche Engpass war immer LUT/FF (Kontroll-/Adresslogik-Overhead bei hoher Serialisierung, siehe Lauf 3→4 oben) — mit dem jetzt kleineren Netz lässt sich das DSP-Budget stärker ausnutzen, um genau diesen Overhead zu vermeiden.

  | Layer | n_in→n_out | RF | DSP (geschätzt) |
  |---|---|---|---|
  | `fc` | 400→36 | 800 | 18 |
  | `fc1` | 36→25 | 36 | 25 |
  | `fc2` | 25→10 | 25 | 10 |
  | conv (`layer1_0`, `layer2_0`) | unverändert | 150 / 2400 | 1 + 1 |
  | **Summe** | | | **~55/80 (69%)** |

  Noch nicht durch echte Vivado-Synthese verifiziert — reine Modell-/RF-Konsistenzrechnung. `export.py` läuft jetzt End-to-End ohne Fehler/Warnungen durch, HLS-Projekt unter `hls4ml/lenet5_project` neu generiert.

## Offene Punkte / Future Work
- **Echte Vivado-Synthese des gepruneten Designs — aktiver nächster Schritt**: `export.py` erzeugt jetzt fehlerfrei ein HLS-Projekt für das Trial-7-Modell (fc 400→36→25→10, RF neu berechnet, ~55/80 DSP geschätzt, siehe oben). Bisher nur Modell-/Config-Konsistenz geprüft, **keine echte Synthese gelaufen** — nächster Schritt: `vsynth=1`-Lauf wie bei Lauf 7 der Vor-Pruning-Iteration, um LUT/FF real zu messen (grobe lineare Schätzung aus dem Pruning-Abschnitt oben war nie verifiziert).
- **io_stream vs. io_parallel**: `io_parallel` liefert grundsätzlich bessere Latenz/Durchsatz (volle Parallelität, keine FIFO-/Streaming-Overhead), skaliert aber schlecht — auf dem sehr kleinen XC7Z010 hat es die Synthese durch komplett partitionierte Zwischenschicht-Arrays gesprengt (24,8GB RAM, Stunden pro Compile-Phase). `io_stream` wurde deshalb aus Notwendigkeit gewählt (Ressourcen-/Synthesierbarkeitsgrund), nicht aus Performance-Gründen. Sobald Precision/Größe durch Quantisierung sitzen: ggf. erneut `io_parallel` mit abgestimmtem `ReuseFactor` probieren (bessere Performance, falls Ressourcen/Synthesezeit es dann zulassen), oder `io_stream`-FIFO-Tiefen (`fifo_opt`) für besseren Durchsatz tunen.
