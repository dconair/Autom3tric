# AutoDJ

Native Android-App für automatisches DJ-Mixing und Remixing mit einer **Granular-Engine** als Herzstück.
Kotlin + Jetpack Compose für die Oberfläche, C++17 (NDK) für den gesamten Audiokern, Oboe/AAudio für die Ausgabe.

## Stand

| Meilenstein | Inhalt | Status |
|---|---|---|
| 1 | Setup, Oboe-Ausgabe, Audio-Thread-Regeln, lock-freie Queues, Parameter-Registry, Transport/BPM-Clock | **fertig** (Host-getestet) |
| 2 | Granular-Prototyp mit Benchmarks 64/128/256/512 Grains, Stresstest | **fertig** (Host-getestet) |
| 2+ | Instrument-Oberfläche nach der genannten Referenz (Cells Hold/Step/Keys, Chaos, Output, dunkler Bildschirm) | **fertig** (Host- und JVM-getestet, nicht auf Gerät) |
| 2++ | macOS-App: dieselbe Instrument-Oberfläche (Compose Multiplatform) auf derselben Engine, CoreAudio-Ausgabe, `.dmg` aus der CI | **gebaut und getestet in der CI, nie auf einem Mac gestartet** |
| 3–9 | Import/Analyse, Beat-Matching, Auto-Mix, Editor, Drumpad, Granular-Feinschliff, Politur | offen, siehe [Roadmap](docs/ARCHITECTURE.md#8-roadmap-meilensteine-aus-der-spezifikation) |

**Ehrlich gesagt:** Die App wurde gebaut (Debug und Release, arm64-v8a + x86_64) und ihre JVM-Unit-Tests laufen, sie wurde
aber **noch nie auf einem Gerät gestartet**. Alle Messwerte in diesem Repository sind **Host-Werte** (x86_64-Xeon, SSE2,
geteilte VM) und keine Gerätewerte. Der NEON-Pfad wurde auf Korrektheit unter QEMU geprüft, nicht auf Geschwindigkeit. Die Zielwerte aus der
Spezifikation (≥ 256 Grains unter 15 % CPU auf Snapdragon 7, Latenz < 20 ms, Touch-zu-Ton < 30 ms) sind **noch nicht nachgewiesen**;
der Benchmark im Debug-Tab der App liefert die Zahlen auf dem Gerät.

## Schnellstart auf dem Rechner (kein Android nötig)

Voraussetzungen: CMake ≥ 3.22, Ninja, ein C++17-Compiler (GCC 13 oder Apple clang), Python 3. GoogleTest und nlohmann/json
werden beim Konfigurieren geladen (gepinnte Tags).

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DAUTODJ_WERROR=ON
ninja -C build-host
ctest --test-dir build-host --output-on-failure        # test_core, test_granular, test_engine, rt_lint (+ Selbsttest)
```

Weitere Konfigurationen, die geprüft wurden: `-DCMAKE_BUILD_TYPE=Debug`, `-DAUTODJ_SANITIZE=address,undefined`,
`-DAUTODJ_SANITIZE=thread`. Mit `-DAUTODJ_RT_GUARD=OFF` entfällt der Allokations-Wächter (für saubere Benchmarks).

```bash
# Benchmark 64/128/256/512 Grains × Callback 128/192 × linear/Hermite × SIMD/skalar  -> Markdown + JSON
build-host/native/bench/bench_granular --md docs/results/bench_host.md --json docs/results/bench_host.json
build-host/native/bench/bench_granular --quick --grains 256 --callbacks 192      # Schnelllauf

# Stresstest: 300 deterministische Fuzz-Seeds, 10 min Audio im Freilauf, 10 min Echtzeit mit Steuer-Thread
build-host/native/tools/stress_test --audio-seconds 600 --paced-seconds 600

# Offline-Render derselben Engine nach WAV (Szenen: pad, shimmer, drums, texture, mix, steps, keys)
build-host/native/tools/render_cli --scene mix --seconds 24 --out mix.wav
```

Hörproben aus diesem Stand liegen als `granular_*.wav` und `instrument_*.wav` (Step- und Keys-Modus) in den Ergebnissen des Threads (nicht im Repository eingecheckt).

## macOS

Die macOS-App liegt in `desktopApp/`: dieselbe Instrument-Oberfläche wie auf Android (Compose Multiplatform teilt die
`androidx.compose`-API; die Dateien werden per `syncSharedSources` aus `app/` übernommen, nicht kopiert), dieselbe
C++-Engine über denselben JNI-Code, aber mit **CoreAudio** statt Oboe (`native/platform/coreaudio/`). Die native Bibliothek
`libautodj_native.dylib` wird universal (arm64 + x86_64) gebaut und in die App gepackt.

**Fertige App herunterladen:** GitHub → *Actions* → neuester Lauf von „CI“ → Artefakt **AutoDJ-macOS** (`.dmg` und `AutoDJ-macos-app.zip`).
Die App ist **nicht signiert und nicht notarisiert**; beim ersten Start blockt macOS sie. Rechtsklick auf die App → *Öffnen*,
oder `xattr -dr com.apple.quarantine /Applications/AutoDJ.app`.

**Selbst bauen** (macOS, Xcode Command Line Tools, CMake, JDK 17 oder neuer):

```bash
./gradlew -PdesktopOnly=true :desktopApp:run                  # startet die App (baut vorher die .dylib)
./gradlew -PdesktopOnly=true :desktopApp:packageDmg           # .dmg unter desktopApp/build/compose/binaries/main/dmg/
./gradlew -PdesktopOnly=true :desktopApp:test                 # JNI-Smoketest gegen die echte .dylib + Offscreen-Render der Oberfläche
```

`-PdesktopOnly=true` lässt das Android-Modul weg (kein Android SDK nötig). Bedienung wie auf Android; Berührung wird zu Mausklick/-ziehen.
Der CI-Lauf baut außerdem den C++-Kern mit Apple clang und führt dort alle Tests aus.

**Ehrlich dazu:** Ich hatte beim Schreiben keinen Mac. Der CoreAudio-Treiber, die `.dylib` und die App sind deshalb nur das, was die CI auf
einem macOS-Runner baut und testet (bei den Audiotests ohne echtes Gerät). Ob Ton herauskommt, wie die Latenz ist und ob die App auf einem
Intel-Mac läuft (der x86_64-Teil wird gebaut, aber nicht ausgeführt), ist **nicht geprüft**.

## Android

Voraussetzungen: Android SDK (Plattform 36), NDK `29.0.14206865`, CMake `3.22.1` (SDK-Manager), JDK 17+.
Gradle 8.14.3 kommt über den Wrapper. `local.properties` mit `sdk.dir=…` anlegen oder `ANDROID_HOME` setzen.

```bash
./gradlew assembleDebug testDebugUnitTest      # Debug-APK (mit RT-Wächter: zählt Verstöße, stürzt nicht ab) + JVM-Tests
./gradlew assembleRelease
```

Oboe 1.10.0 kommt als Prefab-Paket von Google Maven. Die App hat vier Tabs: **Granular** (das Instrument, siehe unten),
**Params** (alle 123 Parameter, generisch aus der Registry erzeugt, Trigger-Pad, Korn-Wolke), **Transport** (Play/Stop/Rewind, BPM, Master,
Testton, Presets kopieren/einfügen) und **Debug** (Last, Deadline-Misses, XRuns, aktive Grains, Qualitätsstufe, Latenz,
Kernel-Backend, exklusiver Modus, Benchmark „Quick/Full“ mit „Copy results“).

### Das Instrument (Tab „Granular“)

Aufbau und Funktionen folgen dem Granular-Instrument, das du als Referenz genannt hast (Details und Abgrenzung in
[ARCHITECTURE.md §5a](docs/ARCHITECTURE.md#5a-instrument-oberfläche-nach-referenz)): dunkler Bildschirm mit Korn-Wolke, Wellenform und
Zellraster (Berührung = Spielen), Modi **hold / step / keys**, 8 oder 16 **Cells** mit Pads, Hüllkurven-Auswahl (6 Formen) und
neun Regler: *space, chaos | length, grains, position | tune, output*. Der große *grains*-Regler steuert die Dichte, im
Sync-Modus die Taktteilung. Name, Farben und Zeichencode sind eigen, es sind keine Marken-Assets übernommen.

| hold | step (16 Cells) | keys + sync |
|---|---|---|
| ![hold](docs/screenshots/instrument_hold.png) | ![step](docs/screenshots/instrument_step16.png) | ![keys](docs/screenshots/instrument_keys_sync.png) |

Die Screenshots entstehen auf der JVM (`./gradlew testDebugUnitTest`, Robolectric + Compose, Ausgabe in `app/build/screenshots`) mit einem
Fake-Host und synthetischen Daten. Sie zeigen das Layout, nicht Schrift oder Rendering eines bestimmten Geräts.

Tests der Oberfläche (19 der 23 JVM-Tests; die übrigen 4 prüfen `ParamSpec`): reine Hilfsfunktionen (Knob-Quantisierung, Hüllkurvenformel = native Formel, Tastennamen), Abgleich der
Instrument-Keys und Bereiche gegen den Registry-Snapshot, und Bedienung über den Accessibility-Baum (Modus-Segmente, Step-Reset,
Pads, Knob per „Fortschritt setzen“ und Doppeltippen, Berührung des Bildschirms, Grains-Regler frei/sync, Hüllkurven-Pfeile).
In Robolectric ruft `captureToImage` den Main-Looper auf, auf dem der Test selbst läuft; die Screenshots zeichnen deshalb das Decor-View direkt in eine Bitmap.

**Erster Gerätelauf (bitte machen):** App starten → Debug-Tab → „Full (~10 min)“ → „Copy results“. Das ergibt die echten
Zahlen für `docs/BENCHMARKS.md`; zusätzlich sollte „RT guard … violations“ im Debug-Build `0` bleiben.

## Verzeichnisse

```
native/core      lock-freie Queues, Parameter-Registry, Presets, Transport, SampleBuffer, RT-Wächter
native/granular  Grain-Pool, Hüllkurven, Kernels (NEON/SSE2/skalar), Scheduler, adaptive Qualität, GranularNode (Cells, Chaos, Output)
native/fx        ChaosFx (Hall → Flanger in einem Regler)
native/engine    Engine (Commands, Blöcke, Master, Status), Testton-Node
native/platform  Audiotreiber + JNI-Brücke: Oboe (Android), CoreAudio (macOS)
native/bench     Benchmark (Host-CLI und App teilen sich den Code)
native/tools     render_cli, stress_test
native/tests     GoogleTest-Suiten + Golden-Datei
app/             Kotlin/Compose (Android)
desktopApp/      macOS-App (Compose Multiplatform), teilt Oberfläche und Engine-Wrapper mit app/
tools/           rt_lint.py (statischer Audio-Thread-Lint)
docs/            ARCHITECTURE.md, AUDIO_THREAD_RULES.md, BENCHMARKS.md, results/, screenshots/
```

## Dokumentation

* [Architekturplan, Entscheidungen, Roadmap](docs/ARCHITECTURE.md)
* [Audio-Thread-Regeln und wie sie durchgesetzt werden](docs/AUDIO_THREAD_RULES.md)
* [Benchmarks und Stresstest: Methode, Zahlen, Grenzen](docs/BENCHMARKS.md)

## Lizenzen der Abhängigkeiten

nlohmann/json (MIT), GoogleTest (BSD-3), Oboe (Apache-2.0). Geplant: Signalsmith Stretch (MIT) für Time-Stretching.
Der Projektcode steht unter der Apache License 2.0 (siehe [LICENSE](LICENSE)).
