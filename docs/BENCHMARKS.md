# Benchmarks, Stresstest und was sie (nicht) beweisen

**Wichtig vorweg:** Alle Zahlen hier sind **Host-Werte** (Intel Xeon 2,1 GHz, 4 vCPUs einer geteilten VM, GCC 13, Release,
SSE2-Kernels). Es gibt **keine Messung auf einem Android-Gerät**. Die Zielwerte der Spezifikation (≥ 256 Grains unter 15 % CPU auf
Snapdragon 7, Latenz < 20 ms, Touch-zu-Ton < 30 ms) sind damit **nicht nachgewiesen**. Der NEON-Pfad wurde nur auf **Korrektheit**
unter QEMU geprüft, nicht auf Geschwindigkeit.

## 1. Methode

* `native/bench/BenchCore.cpp` (gemeinsam für das Host-CLI und den Benchmark-Knopf im Debug-Tab) misst die Wandzeit von
  `Engine::process()` pro Callback, also denselben Aufruf wie der Oboe-Callback. 4 Voices, 48 kHz Stereo-Float, zufällige Positionen
  in einer 20-s-Quelle, Pitch-Streuung ±3 st, Stereo-Spread 0,8, Hüllkurven je Voice verschieden.
* 8 s Messung je Lauf (+ 1,5 s Warm-up), Median aus 3 Läufen; Last = Callback-Dauer / Pufferdauer. Der Governor (adaptive Qualität)
  ist abgeschaltet, damit die gewünschte Last gemessen wird und nicht eine gedrosselte.
* Eigener Build ohne Allokations-Wächter: `cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DAUTODJ_RT_GUARD=OFF`,
  dann `build-bench/native/bench/bench_granular --md docs/results/bench_host.md --json docs/results/bench_host.json`.
  Neu: `--chaos 0.5` misst mit laufendem Chaos-Effekt (Standard 0 = Bypass).
* Die Werte sind nur zwischen Läufen derselben Maschine vergleichbar; auf einer geteilten VM schwanken Ausreißer (max µs).

## 2. Ergebnisse (Host, SSE2)

Vollständige Tabelle (64/128/256/512 Grains × Callback 128/192 × linear/Hermite × SIMD/skalar): [`results/bench_host.md`](results/bench_host.md)
und `.json`.

**Stand dieser Zahlen: vor Einbau der Instrument-Schicht** (Cells, Chaos, Output). Chaos = 0 ist ein bit-identischer Bypass ohne CPU, der
Output-Pegel ist ein Multiplikator pro Block; der Mehraufwand im Standardfall ist damit klein, aber **nicht neu gemessen**. Ein Neulauf
(inklusive `--chaos 0.5`) wurde aus Zeitgründen nicht mehr gemacht und steht aus.

Auszug, Hermite-Interpolation, SSE2, Callback 128 Frames (2,67 ms Budget):

| Grains | mean | p99 | Last mean | Last p99 |
|---:|---:|---:|---:|---:|
| 64 | 41,6 µs | 67,7 µs | 1,6 % | 2,5 % |
| 128 | 77,3 µs | 122,5 µs | 2,9 % | 4,6 % |
| 256 | 145,0 µs | 182,9 µs | 5,4 % | 6,9 % |
| 512 | 308,1 µs | 567,7 µs | 11,6 % | 21,3 % |

Zum Vergleich der skalare Referenzpfad bei 512 Grains (Hermite, 128 Frames): 864 µs mean = 32,4 % (SIMD-Faktor 2,8×).
Mit Callback 192 liegen die Werte bei 256 Grains Hermite SSE2 bei 6,0 % mean / 8,6 % p99, bei 512 Grains bei 10,7 % / 14,5 %.

**Einordnung:** Auf diesem Xeon bleiben 256 Grains bei ca. 5 % und 512 bei ca. 12 % eines Kerns, ein Telefonkern ist langsamer und
anders getaktet (Big/Little, Drosselung). Die Zahl sagt, dass für das 15-%-Ziel Reserve da ist, nicht, dass das Ziel erreicht ist.
Der Benchmark im Debug-Tab liefert die echten Zahlen: Debug → „Full (~10 min)“ → „Copy results“.

## 3. Korrektheit auf ARM (NEON unter QEMU)

`build-aarch64` (Cross-Compiler `aarch64-linux-gnu-g++`, NEON-Kernels), ausgeführt mit `qemu-aarch64` im User-Mode,
Log: [`results/qemu_aarch64_tests.txt`](results/qemu_aarch64_tests.txt). Letzter Lauf auf dem Stand dieses Commits:

| Suite | Tests | Ergebnis |
|---|---:|---|
| test_core | 37 | alle bestanden |
| test_granular | 75 | alle bestanden |
| test_engine | 15 | alle bestanden |

Ausgenommen ist ein einzelner Todes-Test des RT-Wächters (`RtGuardDeathTest.*`), weil `execv` unter der Emulation nicht läuft; er läuft auf dem Host.
Der Kerneltest vergleicht SIMD und skalar gegen ein Double-Precision-Modell (NEON rundet wegen FMA anders als skalar): gemessen max. |SIMD − Modell|
= 1,2·10⁻⁴ bei einem Budget bis 9,8·10⁻⁴. QEMU sagt nichts über Geschwindigkeit oder Timing auf echter Hardware.

## 4. Weitere geprüfte Konfigurationen (Host)

Release, Debug mit AddressSanitizer + UBSan, ThreadSanitizer: `ctest` jeweils grün (test_core, test_granular, test_engine, rt_lint,
rt_lint_self_test; Release zusätzlich `param_specs_snapshot`). Kotlin: 23 JVM-Tests grün (`./gradlew :app:testDebugUnitTest`), Debug-APK gebaut
(arm64-v8a + x86_64), alle 26 JNI-Symbole der gebauten `.so` stimmen mit den Kotlin-Deklarationen überein.

## 5. Stresstest

`native/tools/stress_test`: (0) deterministischer Fuzz mit 300 Seeds × 4000 Callbacks (Parameter-Stürme auf allen 123 Parametern, auch Cells/Chaos/Output, Noten,
Quellenwechsel, Presets, Graph-Tausch, Transport, Resets; Prüfung auf nicht-endliche Ausgabe, Pool-Invarianten und Besitz aller zurückgegebenen
Puffer/Graphen); (1) 10 min Audio im Freilauf mit zufälligen Callback-Größen 1…1500 und einem Control-Thread; (2) 10 min Echtzeit-Takt (192 Frames,
absolute Deadlines; ein Callback länger als sein Puffer zählt als virtueller XRun). Geprüft werden außerdem Allokation im Audio-Thread (Guard)
und Speicherwachstum.

Aufruf: `taskset -c 2,3 build-host/native/tools/stress_test --fuzz-seeds 300 --fuzz-callbacks 4000 --audio-seconds 600 --paced-seconds 600`.

**Ergebnis auf dem finalen Code** (Host, Release mit Guard, Kerne 2+3, vollständig in [`results/stress_host.txt`](results/stress_host.txt)): `STRESS TEST PASSED`.

| Phase | Umfang | Ergebnis |
|---|---|---|
| Fuzz | 300 Seeds × 4000 Callbacks | 0 Fehler (Ausgabe endlich, Invarianten, Besitz) |
| Freilauf | 600 s Audio in 30 s Wand, 1,4 Mio. Callbacks, 3511 Graph-Tausche, 124 Resets | mean 20,6 µs, 0 RT-Verstöße, RSS 43,5 → 49,9 MB, 16 Puffer / 1 Graph am Ende gehalten (Grenze 16 / 2) |
| Echtzeit | 600 s im 192-Frame-Takt, 150 000 Callbacks, 1,38 Mio. Parameter-Schreibzugriffe, 68 710 Graph-Tausche, 2901 Resets | mean 40,6 µs, p99 (gesampelt) 104,8 µs, max 7,1 ms, 0 RT-Verstöße, RSS 47 → 54 MB, nichts hängt am Ende |

**Ehrlicher Vorbehalt:** In der Echtzeit-Phase überschritten **8 von 150 000 Callbacks** ihr Pufferbudget (4 ms), im Freilauf 2 (dort ist der
Wert wegen Callback-Größen ab 1 Frame kaum aussagekräftig). Im selben Lauf war die Aufweck-Verspätung des Betriebssystems bis zu 62 ms; das spricht für
Störungen durch die geteilte VM, ist aber **nicht bewiesen** (Callback-Maximum 7,1 ms). Auf einem Gerät muss das mit dem Debug-Tab und ADPF
neu bewertet werden. Der Test deckt außerdem nur den Pfad ohne echtes Audiogerät ab.

Was der Fuzz früher gefunden hat: eine NaN-Ausgabe bei `loop_start = loop_end = 1` (korrigiert, mit Regressionstest). Außerdem war der
Fuzz zunächst zu schwach (Garbage-Collection und Engine-Reset wurden nie ausgelöst, wegen eines Zählfehlers in der Iterationsnummer);
das ist korrigiert, und der Fuzz prüft jetzt am Ende jedes Seeds, dass keine Puffer/Graphen hängen bleiben.

## 6. Bekannte Grenzen

* Geteilte VM: Zeitwerte schwanken, besonders die Maxima. p99 ist robuster als max.
* Kein Gerätelauf: keine Aussage über Thermik, Big/Little-Scheduling, ADPF-Wirkung, echte Latenz und Treiber-XRuns.
* Die Benchmark-Last ist synthetisch (zufällige Positionen in einer Quelle); echte Musik und Cache-Verhalten können abweichen.
* Der Benchmark deckt den Standardfall (Chaos = 0). Die Kosten von Chaos und Step-Modus sind nicht neu gemessen.
