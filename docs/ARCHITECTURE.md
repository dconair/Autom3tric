# Architekturplan AutoDJ (Stand: Granular-Instrument, Bibliothek, Decks, Auto-Mix, Effekte, Pads; Android + macOS)

> Kurzfassung: Ein plattformunabhängiger C++17-Audiokern (`native/`) enthält Engine, Parameter-Registry, Transport und die
> Granular-Engine. Er läuft identisch im Oboe-Callback (Android), im Offline-Renderer, in den Tests und im Benchmark.
> Kotlin/Compose bedient ihn ausschließlich über eine dünne JNI-Brücke mit lock-freien Queues und Atomics.
> Die Granular-Engine ist das erste vollständige Modul; alles andere (Import, Mix, Editor, Sampler) hängt später als
> weitere `Node`s im selben Graphen und darf sie nicht ausbremsen.

## 1. Modulkarte

```
native/
  core/       Plattform-Grundlagen, keine Abhängigkeit nach oben
              SpscQueue · TripleBuffer · Param (Registry+Store) · ParamSerializer (JSON, Versionen, Migrationen)
              Transport (Master-Clock) · SampleBuffer (Float, 16-byte-ausgerichtet, Guard-Frames) · SourceTable
              Node/GraphDesc · Command · Status · Dsp (xorshift, Softclip, Glättung) · RtGuard · Denormals · DemoSources
  granular/   GrainPool · EnvelopeTables · GrainKernels (NEON/SSE2/skalar) · GrainScheduler · AdaptiveQuality
              GranularParams (Registrierung) · GranularNode (1–4 Voices, Cells Hold/Step/Keys, Chaos, Output)
  fx/         ChaosFx (ein Makro-Regler: Hall → Flanger)
  analysis/   Tempo, Beat-Raster, Tonart, Energie, Aufbau, Peak-Pyramide (Wellenform)
  dj/         Deck (Signalsmith Stretch) · DjDsp (EQ3, Sweep-Filter) · MixPlan/MixPlanner · DjNode (Live-Decks + Plan-Player)
  perf/       MasterFx (Stutter → Roll → Echo → Flanger → Hall) · PadKit/PadNode (synthetische Drum-Pads + Sequencer)
  engine/     Engine (Composition Root: Commands, Blöcke, Master, Status) · Tracks · MixExport (Offline-Render) · ToneNode (Testton)
  platform/   Audiotreiber + JNI: oboe/AudioDriver (Android: Stream, Neustart, LatencyTuner, ADPF) · coreaudio/AudioDriver (macOS: ein AudioUnit) · jni/NativeBridge (gemeinsam)
  bench/      BenchCore (gemeinsam für Host-CLI und App) · bench_granular
  tools/      render_cli (Offline-Render → WAV) · stress_test (Fuzz + Dauerlauf)
  tests/      GoogleTest: core, granular, engine (+ Golden-Datei)
desktopApp/   macOS-App: Compose Multiplatform, teilt Instrument-Oberfläche und Engine-Wrapper mit app/ (Gradle `syncSharedSources`), eigener `NativeLibrary`-Loader und `DesktopHost`
app/          Kotlin/Compose: ViewModel, Instrument-Oberfläche (dunkler Bildschirm, Knobs, Pads), generische Param-Controls aus der Registry, Transport-/Debug-Tab
tools/        rt_lint.py (statischer Audio-Thread-Lint)
docs/         diese Datei, AUDIO_THREAD_RULES.md, BENCHMARKS.md, results/
```

Abhängigkeiten laufen nur nach unten: `fx → core`, `granular → fx, core`, `analysis → core`, `dj → analysis, core`, `perf → fx, core`,
`engine → granular, dj, perf, analysis, core`, `platform/bench/tools/tests → engine`.
Neue Module aus der Spezifikation (analysis, mix-planner, effects, sampler, project-storage) werden zusätzliche Verzeichnisse
neben `granular/`, die nur `core` kennen und als `Node` in den Graphen gehängt werden.

## 2. Threads und Datenfluss

```
 UI-/Control-Thread (Kotlin, Coroutines, MIDI)                 Audio-Thread (Oboe-Callback / Offline-Schleife)
 ───────────────────────────────────────────                   ─────────────────────────────────────────────────
 ParamStore::set()   ── std::atomic<float> je Param ───────▶   Node::setParams()  (1× pro Block, Glättung dort)
 Engine::post(cmd)   ── SpscQueue<Command,1024> ───────────▶   drainCommands()    (Callback-Anfang, max. 256)
 setSource(buffer)   ── Command(Zeiger), Besitz bleibt hier ▶   SourceTable::publish()
 publishGraph(graph) ── Command(Zeiger) ───────────────────▶   Zeiger-Swap (kein Lock, kein Kopieren)
 collectGarbage()    ◀─ SpscQueue<GarbageMsg,256> ──────────    „diesen Puffer/Graph brauche ich nicht mehr“
 readStatus()        ◀─ TripleBuffer<EngineStatus> ─────────    1× pro Callback: Last, Peaks, Beat, Grains, Quality
 readGrainViz()      ◀─ TripleBuffer<GrainViz> ─────────────    ca. 60 Hz: Positionen/Alter/Pan von ≤256 Grains
```

* `Engine::process(out, n)` ist **blockgrößen-agnostisch**: jede Callback-Größe (96, 128, 192, 240, 480 …) wird in gleich
  große Chunks ≤ 192 Frames geteilt (480 → 3×160). Kein Ringpuffer, keine Zusatzlatenz.
* Mehrere Producer (UI, MIDI, Sequencer) serialisieren sich über einen Mutex **auf der Producer-Seite**; der Audio-Thread
  bleibt die einzige Consumer-Seite der SPSC-Queue.
* Der Audio-Graph ist eine unveränderliche `GraphDesc` (bis 16 Nodes mit Gain). Ein neuer Graph wird auf der
  Control-Seite gebaut, per Command übergeben und im Audio-Thread nur als Zeiger getauscht. Der alte geht über die
  Garbage-Queue zurück.
* Regeln, Durchsetzung und Grenzen: [`AUDIO_THREAD_RULES.md`](AUDIO_THREAD_RULES.md).

## 3. Parameter-Registry, Presets

* Jeder steuerbare Wert ist ein `ParamSpec` (stabiler String-Key, Label, Gruppe, Einheit, min/max/default, Typ
  Float/Int/Bool/Enum, Skala linear/logarithmisch, Glättungszeit, Flags). Aktuell 123 Parameter (Snapshot als Test: `app/src/test/resources/param_specs.json`, Abgleich im Build über `param_specs_snapshot`).
* **Alle** Clients nutzen nur die Registry: Kotlin baut die Regler aus `specsToJson()` (Slider, Schalter, Chips,
  Formatierung, Double-Tap = Default); Automation/MIDI/Modulation (M6/M8) adressieren dieselben `ParamId`s.
* Presets sind JSON mit `version`-Feld und Migrationskette; unbekannte Keys werden gemeldet, nicht abgelehnt; Werte
  außerhalb des Bereichs werden geclamped und gemeldet; Defekte JSONs ändern nichts (Test + Stresstest).
* Die Glättung ist Control-Rate: pro Block ein One-Pole-Schritt (Koeffizient aus der echten Blockdauer), Gain-Rampen pro
  Block im Mixer. Nichts davon läuft pro Sample.

## 4. Transport / BPM-Clock

Eine einzige Quelle (`Transport`): `clockBeat` läuft immer frei mit dem BPM-Parameter, `playBeat` ist der Timeline-Playhead
(Play/Stop/Seek). Nodes bekommen pro Block einen `BlockTime` und leiten daraus sample-genaue Offsets ab
(`forEachGridCrossing`). Die halboffenen Intervalle `[clockBeat, clockBeatEnd)` kacheln lückenlos über Blockgrenzen, jede
Rasterlinie wird genau einmal gemeldet (Toleranz 2·10⁻⁹ Beat gegen Rundungsfehler); Abweichung < 1 Sample. Teilungen:
1/4, 1/8, 1/16, 1/32 und Triolen 1/4T, 1/8T, 1/16T.

## 5. Granular-Engine

| Baustein | Umsetzung |
|---|---|
| Grain-Pool | 512 Grains global, Struct-of-Arrays (`pos`, `rate`, `envPos`, `envStep`, `gainL/R`, `remaining` …), Freelist + dichte Active-Liste mit O(1)-Swap-Remove; Iteration absteigend, damit Entfernen beim Durchlaufen sicher ist |
| Scheduler | Dichte als Phasenakkumulator in `double`; der Schwellen-Durchgang wird analytisch gelöst → Grain startet mit exaktem Sample-Offset im Block, Ergebnis unabhängig von der Blockgröße; Timing-Jitter randomisiert die Schwelle (1 ± 0,9·jitter), Mittelwert bleibt erhalten; Dichteänderung wirkt sofort |
| Beat-Sync | gleiche Grain-Erzeugung, aber getriggert von `forEachGridCrossing` der Master-Clock |
| Hüllkurven | 6 Formen (Hann, Gauss, Tukey, Trapez, Rechteck mit 3 %-Rampe, Dreieck; neue Formen hinten angehängt, damit Preset-Indizes stabil bleiben) × 2048 Punkte + Guard, lineare Interpolation, keine Trigonometrie im Inner Loop; LUT-Fehler gemessen < 4·10⁻⁶ |
| Quellen | `SampleBuffer`: Float-Stereo, auf Engine-Rate vorgewandelt, Guard-Frames an beiden Rändern (kein Randtest im Inner Loop); Positionen als `double`, pro Job auf die kleinste berührte Position rebasiert → Float-Genauigkeit unabhängig von der Songposition |
| Interpolation | linear (Standard), Hermite 4-Punkt (Quality); `renderGrain*`-Kernels verarbeiten 4 Frames pro Iteration: NEON (Android, fused `vfmaq`), SSE2 (Host), skalare Referenz. SSE2 ↔ skalar rechnen mit identischer Reihenfolge und stimmen auf ≤ 2·10⁻⁶ überein; NEON rundet wegen FMA anders. Deshalb prüft der Kerneltest beide gegen ein Double-Precision-Modell mit einem Fehlerbudget aus Positions-ULP × Signalsteigung (gemessen: SSE2 und NEON beide max. 1,2·10⁻⁴ bei Budget ≤ 9,8·10⁻⁴) |
| Voices | 1–4, je eigener xorshift-PRNG, eigene Parametergruppe und Quelle (Slot 0–15), Gate-Attack 3 ms, Release einstellbar, Auto-Gain 1/√(überlappende Grains) |
| Stealing | Retrigger, Voice-Limit und Quality-Governor blenden die Grains der Voice über `steal_fade_ms` (Default 4 ms) aus statt abzuschneiden; Test mit Diskontinuitäts-Detektor (2. Differenz) inkl. Empfindlichkeitsprüfung des Detektors |
| Quellenwechsel | `SourceTable` hält pro Eintrag einen Referenzzähler; ein neues Buffer-Publish ersetzt den Slot, laufende Grains lesen ihren alten Puffer zu Ende (Crossfade durch Überlappung); der Puffer geht erst nach dem letzten Grain über die Garbage-Queue zurück |
| Scan / Freeze | Scan-Kopf mit Geschwindigkeit/Richtung und Loop-Fenster (mind. 10⁻⁴ breit, immer in [0,1]), Freeze hält den Kopf |
| Adaptive Qualität | `AdaptiveQuality` (reine State-Machine, ohne Audio testbar): Level 0 voll → 1 nur linear → 2 Dichte-/Grain-Cap 75 % und eine Voice weniger → 3 Cap 50 % und zwei Voices weniger. Runter bei geglätteter Last > 70 % über 100 ms oder sofort bei einem Callback > 90 %; hoch erst nach 1,5 s unter 50 %, mit exponentiellem Back-off gegen Flattern. Last = Callback-Dauer / Pufferdauer, im Offline-Modus abgeschaltet |
| Cells | Die Quelle lässt sich in 8 oder 16 gleich breite Zellen teilen (`cell_count`). *Hold*: Cells ignoriert, `position` überspannt die ganze Quelle. *Step*: die Zelle ist eine reine Funktion des Master-Clock-Beats (`floor((beat − Ursprung) / Schrittlänge) mod n`, der Ursprung ist der Rasterschritt, in dem das letzte `reset` ankam), `position` ist dann der Versatz innerhalb der Zelle, Scan wirkt nur in Hold; dadurch sample-genau und unabhängig von der Blockgröße (Test), ein `StepReset`-Command richtet den Zähler neu aus. *Keys*: `cell_index` (Pad oder MIDI-Taste) wählt die Zelle |
| Chaos / Output | Pro Instanz nach dem Voice-Mix: `ChaosFx` (Freeverb-artiger Hall → modulierter Flanger, ein Regler; bei 0 **bit-identischer** Bypass ohne CPU, Fades laufen ohne abgeschnittenen Hall-Schwanz aus), danach `granular.output_db` (−24 … +6 dB) |
| Sichtbarkeit | Debug-Chip (immer sichtbar) und Debug-Tab: Last, Peak, Deadline-Misses, Driver-XRuns, aktive Grains, Quality-Level, Latenz, Kernel-Backend |

## 5a. Instrument-Oberfläche nach Referenz

Auf Wunsch des Projektinhabers ist das Granular-Instrument **„Il Granello“ von Limo Series** (https://www.limoseries.com/product)
die Referenz für Aufbau, Funktionen und Anmutung. Übernommen wurde die *Struktur*, nicht das Produkt: Name, Logo, Texte,
Farbwerte, Zeichencode und Klangverarbeitung sind eigene Arbeit, es werden keine Marken-Assets kopiert. Was an Details
der Referenz unbekannt ist (z. B. wie ihr „Chaos“ klingt), ist unten als **eigene Interpretation** markiert.

| Referenz (laut Produktseite) | Umsetzung hier | Wo |
|---|---|---|
| Dunkler Bildschirm: Wellenform, Zellraster, Spielkopf, Körner als Wolke | `WaveformScreen`: oben Korn-Wolke (eine Spur je Voice), unten Wellenform mit Zellraster und Tasten-Labels, Spielkopf je Voice, helle Platte hinter der gerade gelesenen Zelle; Berührung = Spielen, Ziehen = Position/Zelle | `app/.../ui/WaveformScreen.kt` |
| Modi Hold / Step / Keys | `cell_mode`; Hold = Position frei, Step = Zellen laufen im Takt (Teilung 2 Takte … 1/16, `reset`-Knopf), Keys = Pads/Tasten wählen die Zelle | `GranularNode`, `InstrumentScreen.kt` |
| 8 oder 16 Zellen mit Pads | `cell_count`, `PadKey` (Name der Taste ab C3) | dito |
| Regler: space, chaos \| length, **grains**, position \| tune, output | `spread`, `granular.chaos`, `size_ms`, `density` (frei) bzw. `sync_div` (Sync), `position`, `pitch_st` (rastet auf Halbtöne), `granular.output_db`; der große Regler wechselt zwischen Dichte und Taktteilung je nach sync/free | `InstrumentScreen.kt` |
| Hüllkurven-Auswahl mit Pfeilen | `EnvelopeSelector` zeichnet die Form; 6 Formen, die Zeichnung nutzt dieselbe Formel wie `EnvelopeTables` (Test) | `InstrumentControls.kt` |
| Zurückhaltende Anmutung (warmes Beige, dunkle Knöpfe, Kleinschreibung) | eigene Palette in `Theme.kt`; Knobs: Ziehen = ändern, Doppeltippen = Default, TalkBack-Aktion „Fortschritt setzen“ | `Theme.kt`, `Knob` |
| „Chaos“-Regler | **Eigene Interpretation:** ein Makro, das von kurzem Hall über Hall+Flanger zu überwiegend Flanger überblendet (`ChaosFx`) | `native/fx` |

Bewusst noch nicht dabei: Drag-and-drop/Import eigener Dateien (kommt mit M3, bis dahin vier synthetische Quellen), Modulations-Matrix
und XY-Pad (M8), Einbindung als Effekt/Übergang (M8). Die Oberfläche läuft gegen das Interface `InstrumentHost`, damit
Tests ohne Gerät eine echte Registry mit Fake-Host bedienen können (`FakeInstrumentHost`); Screenshots entstehen so auf der
JVM (Robolectric, `docs/screenshots/`) und zeigen das **Layout**, nicht die Schrift oder das Rendering eines bestimmten Geräts.

## 5b. macOS

Ziel: dieselbe App auch auf dem Mac. Weg: Kern und Oberfläche bleiben gemeinsam, nur die Plattformschicht ist getauscht.

| Schicht | Android | macOS |
|---|---|---|
| Audio-Ausgabe | Oboe/AAudio | CoreAudio (`kAudioUnitSubType_DefaultOutput`, Float-Stereo, folgt dem Standardgerät) |
| JNI-Brücke | `libautodj_native.so` | `libautodj_native.dylib` (universal arm64 + x86_64), derselbe `NativeBridge.cpp` |
| Oberfläche | Compose (AndroidX) | Compose Multiplatform Desktop, **dieselben** Dateien für Instrument, Regler und Bildschirm |
| Host | `AutoDjViewModel` | `DesktopHost` (gleiches `InstrumentHost`-Interface, Polling aus der UI) |
| Paket | APK | `.dmg` (jpackage über das Compose-Plugin), unsigniert |

Entscheidungen: Compose Multiplatform statt SwiftUI, weil die Oberfläche dann nicht doppelt gepflegt wird und derselbe getestete Code läuft;
CoreAudio statt Oboe, weil Oboe nur Android unterstützt; die geteilten Kotlin-Dateien werden per Gradle-Sync übernommen, damit es keine Kopien gibt.
Der Preis: Die macOS-App braucht eine JVM (im `.dmg` mitgeliefert, deshalb ca. 100 MB), und ein späterer reiner Swift-Port wäre ein neues Projekt.

Grenzen (ehrlich): Der RT-Wächter ist in der macOS-Bibliothek aus (Release). CoreAudio-Latenz wird aus Puffergröße, Gerätelatenz und Safety-Offset
geschätzt, nicht gemessen. Ob der Treiber unter Last, bei Gerätewechsel (Kopfhörer) und auf Intel-Macs gut läuft, ist ungeprüft; die CI hat keine
Audiohardware. Keine Signierung/Notarisierung (dafür braucht es ein Apple-Developer-Konto).

## 5c. Bibliothek, Import und Analyse (M3)

Mehrere Songs in eine Liste laden; jeder wird dekodiert, analysiert und mit Wellenform in der Bibliothek angezeigt.

| Schritt | Wo | Was |
|---|---|---|
| Dateiwahl | Android `OpenMultipleDocuments` (dauerhafte URI-Rechte), Mac `FileDialog` (Mehrfachauswahl) | Pfade/URIs gehen als Strings an den `DjController` |
| Dekodieren | Android `MediaExtractor`+`MediaCodec` (`AndroidTrackDecoder`); Mac WAV direkt, sonst `afconvert` (MP3, M4A/AAC, FLAC, AIFF), danach Java-Sound-Provider (MP3, FLAC, Ogg), zuletzt `ffmpeg` falls vorhanden (`DesktopTrackDecoder`) | liefert Float-PCM in Blöcken an `PcmSink` |
| Import in den Kern | JNI `nativeIngest*`, `TrackIngest` | Mono wird verdoppelt, Surround auf Stereo gemischt, Kaiser-Sinc-Resampler (`Resampler`) auf die Engine-Rate |
| Analyse | `native/analysis` | Tempo (Onset-Autokorrelation, 75–185 BPM, Prior um 124), Beat-Raster (robuste Geradenanpassung an Onsets), erster Taktschlag (Bass-Fluss), Tonart (Chroma + Krumhansl-Schmuckler, Camelot), Energie je Takt, Abschnitte (Intro/Drop/Break/Outro), Mix-Ein-/Ausstiegspunkt, Lautheit/Trim |
| Wellenform | `PeakPyramid` (Blöcke à 64, ×4-Stufen) | Übersicht für die Liste, später Zoom in den Decks |
| Speicher | `JsonLibraryStore` (`library.json`) | nur Analyse und Übersicht, nie das Audio; beim Laden wird ein Song neu dekodiert, die Analyse entfällt |

Abweichung: Die Bibliothek liegt als JSON, nicht in Room, damit sie auf Android und Mac derselbe Code ist. Die Analyse wurde mit synthetischen Songs
(Tempo, Tonart, Aufbau bekannt) und mit Rauschen/Stille getestet, **nicht mit echter Musik** – wie gut sie bei Live-Aufnahmen, Swing oder
Tempowechseln ist, zeigt erst ein Test mit deinen Songs. Songs ohne erkennbares Tempo bleiben importierbar und werden als solche markiert.

## 5d. DJ-Kern: Decks, Sync, Auto-Mix, Export

Alles Hörbare am DJ-Teil ist ein einziger `DjNode` im Graphen mit zwei Decks. Er hat zwei Betriebsarten:

* **Live** (Tab „Decks“): Die Decks werden von Parametern (`deck.a.*`, `deck.b.*`, `mixer.*`: Lautstärke, Trim, EQ low/mid/high in dB mit −26 = Kill, Filter −1…+1,
  Tempo ±16 %, Sync, Key-Lock, Crossfader + Kurve) und Commands (Laden, Play, Seek) gesteuert. **Sync** macht ein Deck zum Folger: sein Tempo
  wird auf das des anderen gesetzt und seine Beat-Phase über eine begrenzte Tempo-Korrektur (max. ±6 %) eingezogen, nie durch einen Sprung.
* **Plan** (Tab „Auto-Mix“): Ein unveränderlicher `MixPlan` bestimmt, welcher Song auf welchem Deck von welchem Master-Beat bis zu welchem läuft, wie sich
  das Master-Tempo ändert (stückweise linear in Beats, mit geschlossenen Formeln für die Zeit) und die Automationsspuren (Lautstärke, EQ, Filter je Deck).
  Jeder Song ist an das Master-Raster „eingerastet“, die Quellposition ist also eine reine Funktion der Master-Beats; das Deck verfolgt sie mit begrenzter Korrektur.

**Deck.** Signalsmith Stretch (MIT) mit Key-Lock. Der Start geht über `outputSeek`, der erste Ausgabewert entspricht damit schon der gewünschten Quellposition (keine Anlaufzeit).
Pausieren blendet kurz aus und merkt sich die Stelle; ein Seek während des Ausblendens parkt das Deck am neuen Ort (`restAt_`).

**EQ/Filter.** Dreiband-EQ aus Linkwitz-Riley-Weichen (LR4) mit phasenkompensiertem Tiefbass-Zweig, damit „flach“ wirklich flach ist; Kill dämpft ≥ 26 dB. Sweep-Filter: Tiefpass links, Hochpass rechts, Mitte = aus.

**Übergänge.** Bass-Swap (der Bass des neuen Songs bleibt bis zur Mitte der Überblendung weg, dann tauschen die Bässe), Crossfade (gleiche Lautstärke), Filter-Sweep, Schnitt; „Automatisch“
wählt je Paar nach Tempo, Tonart (Camelot) und Länge der Songs. Tempi, die nach Halbieren/Verdoppeln mehr als 12 % auseinanderliegen, bekommen einen Schnitt auf dem Taktanfang.
Die Reihenfolge lässt sich optimieren (Smart: ähnliches Tempo, passende Tonart, ähnlicher Pegel; Tempo auf-/absteigend).

**Nachladen.** Ein Mix aus vielen Songs darf nicht als dekodiertes Float-Audio im Speicher liegen. Der Plan wird deshalb aus Analyse-Stubs gebaut (`Track::audio == nullptr`);
die Kotlin-Seite dekodiert Song *i+1* vor seinem Einsatz und hängt ihn mit `bindAudio` ein, der Audio-Thread liest nur einen atomaren Zeiger (`PlanAudio`). Kommt ein Song zu spät,
bleibt sein Deck still und er steigt ein, sobald er da ist („waiting“). Freigegeben wird erst, wenn der Audio-Thread garantiert fertig ist.

**Export.** `MixExporter` rendert den Plan mit einer eigenen, nicht-echtzeitfähigen Engine in Schritten (Kotlin bindet zwischen den Schritten die nächsten Songs) als 16-Bit-WAV mit TPDF-Dither oder 24-Bit,
dazu `<Name>-Tracklist.txt`. Der Export klingt wie die Wiedergabe (Test: gleiche Plan-Position, gleiche Pegel).

Grenzen: Der Planer kennt nur, was die Analyse liefert (Tempo, Raster, Tonart, Energie, Aufbau); bei Live-Aufnahmen, Swing oder Tempowechseln im Song kann das Raster danebenliegen.
Die Tests laufen mit synthetischen Songs, nicht mit echter Musik. Kein Transients-Erhalt-Tuning, kein Stem-/Vocal-Handling.

## 5e. Performance: Effekte, Drum-Pads, Freeze

Zwei neue Teile im Graphen und eine gemeinsame Taktquelle.

**MasterClock** (`core/MasterClock.h`): wo das Beat-Raster der Musik in diesem Block liegt. Läuft ein Plan, ist es dessen Master-Uhr; sonst das Raster des lauteren spielenden Decks
(Beat aus seiner Wiedergabeposition, Tempo × aktuelle Rate); sonst die frei laufende Transport-Uhr (`transport.bpm`). Die Uhr wird einmal je Chunk **vor** den Knoten berechnet
(`DjNode::masterClock`) und per `ProcessIO::clock` an alle Knoten gegeben.

**MasterFx** (`perf/`): Reihe auf dem Summenbus vor dem Master-Fader: Stutter (Gate auf dem Raster) → Beat Roll (die letzten ¼–1 Beat werden gehalten und wiederholt, Nahtstellen mit 1-ms-Fades) → Echo
(Ping-Pong, Abstand in Beats, Rückkopplung mit Tiefpass, Fahne klingt nach dem Ausschalten aus und wird danach einmal geleert) → Flanger und Hall (`ChaosFx` an seinen beiden Enden).
Alle Effekte aus = **bitgleicher** Durchgang (Test). Alles ist vorab allokiert; Kosten messbar < 1 % Echtzeit auf dem Host.

**Pads** (`PadKit`, `PadNode`): 16 Klänge werden beim Start synthetisiert (keine Sample-Dateien, daher auf beiden Plattformen gleich, ohne Lizenzfragen). 24 Stimmen; Hi-Hat und Open Hat sind eine Choke-Gruppe.
Der Sequencer hat 16 Zeilen × 16 Steps (Sechzehntel; Step 1 = Taktanfang des Rasters) und löst Treffer sample-genau im Chunk aus; ein Sprung der Uhr (Seek, Deckwechsel) startet den Lauf neu, ohne Doppel- oder Fehltreffer (Test).
Das Muster ist Zustand des Knotens und wird mit `PadRow`-Commands gesetzt, Treffer von Hand mit `PadTrigger`. Swing verschiebt jeden zweiten Step um bis zu 30 % eines Steps.

**Freeze („Granular als Effekt“):** Die Engine schreibt die letzten 10 s des Ausgangs in einen Ringpuffer aus relaxed Atomics; `captureOutput` (Control-Thread) kopiert bis zu 8 s daraus in einen neuen `SampleBuffer`
(bitgleich zum Gerenderten, Test), der als normale Granular-Quelle gesetzt wird. Auf dem Audio-Thread kostet das nur das Schreiben in den Ring.

Grenzen: Kein Sampler (eigene Klänge auf Pads), keine Velocity-/Aufnahmefunktion für die Pads (die Steps haben nur an/aus plus ein festes Akzentprofil für Hats/Percussion), die Effekte haben keine Automation.

## 6. Wichtige Entscheidungen

| Entscheidung | Begründung | Alternative / Preis |
|---|---|---|
| C++-Kern ohne Android-Abhängigkeit, Oboe nur im `platform/`-Teil | Tests, Benchmarks, Export und Sanitizer laufen auf dem Host; Offline = gleicher Code | etwas mehr CMake |
| Gleich große Chunks ≤ 192 statt festem Block + FIFO | keine zusätzliche Latenz, jede Burst-Größe funktioniert | Steuerrate (Glättung) hängt von der Callback-Größe ab → Export klingt nicht Sample-identisch zu jeder Vorschau-Blockgröße. Gleiche Blockgröße ist bitgleich (Test), verschiedene sind gleich laut/dicht, aber andere „Zufallsrealisierung“ (Test) |
| Freelist + dichte Active-Liste statt nur Freelist | Inner Loop iteriert ohne Lücken, Entfernen O(1) | Reihenfolge der Grains ändert sich (mischt addierend, hörbar irrelevant) |
| `Command` ≤ 32 Byte, nur Zeiger auf unveränderliche Objekte | Queue bleibt trivial kopierbar; Besitz klar bei der Control-Seite | Control-Seite muss `collectGarbage()` regelmäßig aufrufen (UI-Poll macht das) |
| Ein `ParamStore`-Atomic pro Parameter, relaxed | wartefrei, keine Queue-Flut bei Slider-Bewegungen | Mehrere zusammengehörige Werte sind nicht atomar als Gruppe; für Granular unkritisch (pro Block geglättet) |
| JSON (nlohmann) nur Control-Thread | Komfort ohne Audio-Risiko; Presets/Specs/Projekt | Parsing nie im Callback |
| Oboe über Maven/Prefab (1.10.0) | gepflegte Version, 16-KB-Seiten-Support über NDK 29 | Abhängigkeit von Google Maven beim Build |
| Compose Multiplatform + CoreAudio für macOS | siehe §5b | JVM im Paket, unsigniert |
| minSdk 26, arm64-v8a + x86_64 | AAudio ab 26 (Oboe fällt darunter auf OpenSL zurück, nicht nötig); armeabi-v7a bewusst weggelassen | ältere 32-bit-Geräte nicht unterstützt |
| Debug-Build mit RT-Guard, der **zählt statt abzustürzen** | erster Gerätelauf soll Verstöße sichtbar machen, nicht die App beenden | Host-Tests/Stresstest brechen weiterhin hart ab |

## 7. Abweichungen von der Spezifikation und offene Punkte

* **Nicht auf einem Gerät ausgeführt.** Die Android-App wurde gebaut (`assembleDebug`/`assembleRelease`, arm64-v8a +
  x86_64) und die 26 JNI-Symbole (aus der gebauten `.so` per `nm -D`) gegen die Kotlin-Deklarationen abgeglichen, aber nie gestartet. Alle Messwerte sind
  **Host-Werte** (x86_64-Xeon, SSE2). Das NEON-Pfad wird per QEMU auf Korrektheit geprüft (siehe BENCHMARKS.md), nicht auf
  Geschwindigkeit. Die Zielwerte (≥ 256 Grains < 15 % auf Snapdragon 7, Latenz < 20 ms, Touch-zu-Ton < 30 ms) sind damit
  **noch nicht nachgewiesen**; der Benchmark-Knopf im Debug-Tab liefert die Zahlen auf dem Gerät.
* Das Granular-Instrument hat vier synthetische Quellen (`DemoSources`) und nimmt zusätzlich Songs aus der Bibliothek (Slots 4–15).
* Audio-Fokus, Foreground-Service und Kopfhörer-Routing-UI fehlen noch (Stream-Neustart bei Disconnect ist drin).
* Perfetto/Simpleperf-Profiling: Hooks nicht nötig (reiner C++-Aufruf), Anleitung folgt zusammen mit den ersten Gerätedaten.
* Nodes außer Granular/Testton, Modulations-Matrix (der Hook steht in `GranularNode::setParams`) und Projekt-JSON folgen.

## 8. Roadmap (Meilensteine aus der Spezifikation)

| M | Inhalt | Was dafür schon steht |
|---|---|---|
| 1 ✔ | Setup, Oboe, Audio-Thread-Regeln, Queues, Registry, Transport | `core/`, `platform/`, Guard, Lint, Tests |
| 2 ✔ | Granular-Prototyp mit Benchmarks | `granular/`, `bench/`, Stresstest |
| 3 ✔ | Import, Dekodierung (MediaCodec / afconvert / Java Sound), Wellenform, Analyse (BPM, Beat-Grid, Tonart, Energie, Aufbau) | siehe 5c; synthetisch getestet, nicht mit echter Musik |
| 4 ✔ | Wiedergabe mit Beat-Matching, Crossfade (Decks, Sync, EQ, Filter) | siehe 5d; synthetisch getestet |
| 5 ✔ | Auto-Mix, Übergangstypen, Offline-Export | siehe 5d |
| 6 ◐ | Effekt-Rack ✔ (Echo, Hall, Flanger, Roll, Stutter, siehe 5e); Timeline-Editor und Automation offen | Registry (`automatable`-Flag), Command-Queue, Projekt-JSON nach Preset-Muster |
| 7 ◐ | Drumpad ✔ (siehe 5e); Sampler mit Auto-Slicing offen | `noteOn/noteOff`, `PadNode` als Vorlage |
| 8 ◐ | Granular fertig: Modulations-Matrix, XY-Pad; Einbindung als Effekt per Freeze ✔, als Übergang offen | Control-Rate-Hook, `modulatable`-Flag, Adaptive Qualität fertig |
| 9 | Politur, Tests, README mit Benchmarks | Benchmark-Runner in App und Host, `docs/results/` |

## 9. Fragen, die erst später echte Weichenstellungen werden

1. **Lizenz Time-Stretching/Stems:** Signalsmith Stretch (MIT) ist vorgesehen. Rubber Band wäre GPL (App müsste dann GPL
   sein oder eine kommerzielle Lizenz kaufen), SoundTouch LGPL (dynamisch linken). Ich gehe von **Signalsmith (MIT)** aus.
2. **Quellen für Songs:** nur lokale Dateien (wie spezifiziert). Streaming-Dienste mit DRM lassen sich nicht dekodieren
   und mischen; das ist keine technische, sondern eine Rechtefrage.
3. **Stem-Separation (ONNX/Demucs):** Modellgröße und Lizenz des Modells entscheiden, ob es mit in den APK kommt oder
   nachgeladen wird. Bleibt „optional später“ und klar vom Kern getrennt.
