# Audio-Thread-Regeln

Der Audio-Thread ist der Oboe-Callback und alles, was er aufruft: `AudioDriver::onAudioReady` → `Engine::process` →
Nodes (`GranularNode`, `ToneNode`, später Mixer/Effekte). Eine Überschreitung der Deadline ist ein hörbarer Knacks.
Deshalb gelten dort Regeln, die nicht „nach Möglichkeit“, sondern **immer** gelten.

## Verboten

| Verboten | Warum | Stattdessen |
|---|---|---|
| `new`, `delete`, `malloc`, `make_unique/shared`, `std::vector::push_back/resize`, `std::string`, `std::map` … | Heap-Lock im Allokator, unbeschränkte Dauer | alles in `prepare()` vorallozieren; feste Arrays (`GrainPool`, `SpscQueue`) |
| `std::mutex`, `lock_guard`, `condition_variable`, `sleep`, `future` | Prioritätsinversion, blockiert | Atomics, `SpscQueue`, `TripleBuffer` |
| JNI-Aufrufe, `JNIEnv`, Callbacks in die JVM | kann GC-/Safepoint-blockieren | Status per `TripleBuffer`, Kotlin pollt |
| Logging (`printf`, `__android_log_*`, iostream) | Syscall/Lock | Zähler im Status-Snapshot, Anzeige im Debug-Tab |
| Datei-/Netzwerk-I/O, `mmap`, `fopen` | Syscall, Page-Faults | Decoder/Sample-Manager laden im Control-Thread und übergeben fertige Puffer |
| Exceptions (`throw`, `try`) | Allokation + unvorhersehbare Dauer | `noexcept`, Fehlercodes, Clamping an den Rändern |
| `std::shared_ptr`-Kopien, `std::function` | atomarer Refcount / Heap | rohe Zeiger auf unveränderliche Objekte + Garbage-Queue |
| `dynamic_cast`, RTTI | unnötig, teilweise langsam | virtuelle Funktionen oder Templates |

## Erlaubt

Atomics (`relaxed`/`acquire`/`release`), die lock-freien Strukturen aus `native/core`, vorallozierte Arrays, Mathematik,
`clock_gettime` (vDSO, kein Syscall), `AUTODJ_RT_FORBIDDEN(...)`-Marker.

## Wie Daten über die Thread-Grenze kommen

```
UI / Control-Thread                                   Audio-Thread
------------------------------------------------------------------------------------------
ParamStore::set()  (ein std::atomic<float> pro Param) -> setParams() liest einmal pro Block
Engine::post(Command)  (SpscQueue, Mutex nur auf der Producer-Seite) -> drainCommands() zu Callback-Beginn
setSource(shared_ptr<SampleBuffer>)  -> Command mit rohem Zeiger; Besitz bleibt in Engine::heldBuffers_
publishGraph(unique_ptr<GraphDesc>)  -> Zeiger-Swap im Audio-Thread; alter Graph wandert in die GarbageQueue
collectGarbage() <- GarbageQueue      (Audio-Thread gibt nie frei, er meldet nur „fertig“)
readStatus()/readGrainViz() <- TripleBuffer (wartefrei, immer der neueste Snapshot)
```

Regel dahinter: **Der Audio-Thread besitzt nichts, was er freigeben müsste.** Puffer und Graphen gehören der
Control-Seite; der Audio-Thread meldet über die `GarbageQueue`, wann er sie nicht mehr braucht (bei Samples erst, wenn
auch das letzte Korn, das darauf liest, zu Ende ist: `SourceTable` zählt Referenzen).

## Wie die Regeln durchgesetzt werden

1. **Laufzeit-Guard** (`RtGuard.h`, `RtAllocGuard.cpp`): `Engine::process` setzt einen `AudioThreadScope`. In Builds mit
   `AUTODJ_RT_GUARD=1` (alle Host-Tests, Stresstest, **Android-Debug**) ersetzen wir global `operator new/delete`; jede
   Allokation oder Freigabe im Scope ruft den Violation-Handler. Tests und Host-Tools brechen ab, die Android-Debug-App
   loggt nach Logcat und zählt (Debug-Tab: „RT guard … violations“).
2. **Statischer Lint** (`tools/rt_lint.py`, läuft als CTest `rt_lint`): sucht in den als Audio-Thread-Code deklarierten
   Dateien/Funktionen nach den verbotenen Konstrukten aus der Tabelle. Ausnahmen nur per `// rt-lint: allow <Grund>`.
   `rt_lint.py --self-test` beweist, dass jede Regel feuert.
3. **Typen**: `SpscQueue` verlangt `std::is_trivially_copyable`; Commands sind reine Datenstrukturen.
4. **Tests**: alle Engine-Tests laufen mit aktivem Guard; TSan-Lauf (`-DAUTODJ_SANITIZE=thread`) für die
   Mehr-Thread-Tests; der Stresstest hämmert gleichzeitig vom Control-Thread auf die Engine.
5. **Numerik**: `ScopedFlushDenormals` (FTZ/DAZ via MXCSR bzw. FPCR) pro Callback; `-ffp-contract=off` damit skalare
   Referenz und SIMD-Pfad vergleichbar bleiben; der Master-Ausgang hat einen Soft-Clip, NaN/Inf-Eingaben werden an
   den Parametergrenzen abgefangen (`ParamRegistry::clamp/fromNormalized`).

## Checkliste für neue Nodes / Features

- [ ] Alle Puffer in `prepare()`/Konstruktor, keine Größenänderung im Callback.
- [ ] Neue Datei/Funktion in `RT_FILES` / `RT_FUNCTIONS` von `tools/rt_lint.py` eintragen.
- [ ] Parameter über die Registry (`ParamSpec`), niemals eigene Atomics an der Registry vorbei.
- [ ] Eingaben, die aus Presets/UI kommen, sind geclamped; Divisionen und `log`/`sqrt` haben definierte Grenzen.
- [ ] Test, der die Node durch `Engine::process` laufen lässt (damit der Guard greift), plus ein Fuzz-Lauf im Stresstest.
- [ ] Neue Objekte, die der Audio-Thread nutzt: Besitz auf der Control-Seite, Rückgabe über `GarbageQueue`.

## Bekannte Grenzen (ehrlich)

- Der Guard sieht nur `operator new/delete`. Direkte `malloc`-Aufrufe in Drittbibliotheken, Syscalls und Locks außerhalb
  von `AUTODJ_RT_FORBIDDEN` sieht er nicht; dafür gibt es den Lint und das Review.
- Der Lint ist textbasiert (Regex auf entkommentiertem Code), kein Compiler-Frontend. Er findet keine Allokation, die in
  einer aufgerufenen Funktion außerhalb der deklarierten Dateien steckt.
- Allokationen innerhalb von Oboe/AAudio laufen außerhalb unseres Scopes und werden nicht erfasst.
- Release-Builds enthalten keinen Guard (kein Overhead). Auf dem Gerät wurde der Guard in dieser Session **nicht**
  ausgeführt (kein Gerät verfügbar); auf dem Host laufen Tests und Stresstest damit ohne Verstöße.
