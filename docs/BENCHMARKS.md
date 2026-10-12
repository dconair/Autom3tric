# Benchmarks, Stresstest und was sie (nicht) beweisen

**Wichtig vorweg:** Alle Zahlen hier sind **Host-Werte** (Intel Xeon 2,1 GHz, 4 vCPUs einer geteilten VM, GCC 13, Release,
SSE2-Kernels). Es gibt **keine Messung auf einem Android-Gerät**. Die Zielwerte der Spezifikation (≥ 256 Grains unter 15 % CPU auf
Snapdragon 7, Latenz < 20 ms, Touch-zu-Ton < 30 ms) sind damit **nicht nachgewiesen**. Der NEON-Pfad wurde nur auf **Korrektheit**
unter QEMU geprüft, nicht auf Geschwindigkeit.

## 1. Methode

* `native/bench/BenchCore.cpp` (gemeinsam für das Host-CLI und den Benchmark-Knopf im Debug-Tab) misst die Wandzeit von
  `Engine::process()` pro Callback, also denselben Aufruf wie der Oboe-Callback. 4 Voices, 48 kHz Stereo-Float, zufällige Positionen
  in einer 20-s-Quelle, Pitch-Streuung ±3 st, Stereo-Spread 0,8, Hüllkurven je Voice verschieden.
* 8 s *gerendertes Audio* je Lauf (+ 1,5 s Warm-up; die Läufe dauern wegen der Rendergeschwindigkeit nur Sekunden), Median aus 3 Läufen; Last = Callback-Dauer / Pufferdauer. Der Governor (adaptive Qualität)
  ist abgeschaltet, damit die gewünschte Last gemessen wird und nicht eine gedrosselte.
* Eigener Build ohne Allokations-Wächter: `cmake -S . -B build-bench -G Ninja -DCMAKE_BUILD_TYPE=Release -DAUTODJ_RT_GUARD=OFF`,
  dann `build-bench/native/bench/bench_granular --md docs/results/bench_host.md --json docs/results/bench_host.json`.
  Neu: `--chaos 0.5` misst mit laufendem Chaos-Effekt (Standard 0 = Bypass), z. B. `--chaos 0.5 --kernel simd --md docs/results/bench_host_chaos.md`.
* Die Werte sind nur zwischen Läufen derselben Maschine vergleichbar; auf einer geteilten VM schwanken Ausreißer (max µs).

## 2. Ergebnisse (Host, SSE2)

Vollständige Tabelle (64/128/256/512 Grains × Callback 128/192 × linear/Hermite × SIMD/skalar): [`results/bench_host.md`](results/bench_host.md)
und `.json`.

Stand: gemessen auf dem finalen Code (mit Cells, Chaos, Output), ruhige Maschine, ein Kern fest zugewiesen (`taskset`), Build ohne Wächter.
Die Zahlen vor dem Instrument-Umbau lagen innerhalb von rund ±10 % (z. B. 256 Grains Hermite SSE2, 128 Frames: 145,0 µs damals, 159,7 µs jetzt;
auf einer geteilten VM ist das Rauschen in dieser Größenordnung).

Auszug, Hermite-Interpolation, SSE2, Callback 128 Frames (2,67 ms Budget), Chaos aus:

| Grains | mean | p99 | Last mean | Last p99 |
|---:|---:|---:|---:|---:|
| 64 | 38,3 µs | 66,2 µs | 1,4 % | 2,5 % |
| 128 | 77,8 µs | 199,9 µs | 2,9 % | 7,5 % |
| 256 | 159,7 µs | 264,1 µs | 6,0 % | 9,9 % |
| 512 | 317,5 µs | 459,3 µs | 11,9 % | 17,2 % |

Zum Vergleich der skalare Referenzpfad bei 512 Grains (Hermite, 128 Frames): 821,5 µs mean = 30,8 % (SIMD-Faktor 2,6×).
Mit Callback 192 (4 ms Budget) liegen 256 Grains Hermite SSE2 bei 5,5 % mean / 7,0 % p99, 512 Grains bei 11,8 % / 16,4 %.

**Chaos-Effekt** ([`results/bench_host_chaos.md`](results/bench_host_chaos.md), `--chaos 0.5`, Hall und Flanger laufen): Mehraufwand ca. 8–13 µs pro
128-Frame-Callback bei 64–256 Grains (≈ 0,3–0,5 % des Budgets), bei größeren Lasten im Rauschen. Mit Chaos 0,5: 256 Grains Hermite 6,0 % mean / 8,0 % p99,
512 Grains 12,2 % / 23,4 %.

**Einordnung:** Auf diesem Xeon bleiben 256 Grains bei ca. 6 % und 512 bei ca. 12 % eines Kerns, ein Telefonkern ist langsamer und
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
* Gemessen sind Chaos aus und Chaos 0,5. Der Step-Modus (Zellberechnung pro Grain) hat keine eigene Messreihe; er ist rechnerisch klein (eine Division und ein Modulo pro neuem Grain), aber nicht separat belegt.
