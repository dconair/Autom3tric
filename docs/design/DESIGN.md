# Auto DJ: Designsprache "Kabine" (Version 2)

Stand 2026-10-15. Ersetzt die erste Fassung (helles Knochen-Grün-Brett, ins Archiv verschoben nach `archiv/v1-knochen/`). Gilt für Desktop (macOS) und Android. Diese Datei ändert keinen Code; der Code-Thread setzt sie um. Anklickbar: [mockups/prototype.html](mockups/prototype.html) (im Browser öffnen, Tabs, Pads und Halten-Flächen reagieren, die Scheiben drehen sich im Takt). Bilder: [screens/](screens/).

## 1. Warum die zweite Fassung

Rückmeldung von dav1d: moderner, **DJ-mäßiger**, nicht wie eine Kopie der Referenzen, ein eigener Look. Die erste Fassung war ein helles Hardware-Brett und lag damit zu nah am beigen Granular-Vorbild. Die neue Fassung geht den anderen Weg: Sie nimmt, was ein DJ-Pult von anderen Oberflächen unterscheidet (dunkle Kabine, Plattenteller, Signal-Licht, Taktzähler) und baut daraus einen eigenen Auftritt.

Granular ist jetzt kein Vorbild mehr, sondern ein Tab wie die anderen, im selben Material.

## 2. Der Look in einem Satz

**Schwarze Kabine, Knochenweiß und ein einziges Signalorange; die Platte ist eine Taktscheibe, auf der eine Umdrehung genau ein Takt ist.**

Eigenes, das es so nur hier gibt:

1. **Die Taktscheibe.** Jedes Deck zeigt einen Plattenteller. Um die Nabe liegt ein Ring aus 180 Strichen: der Pegelverlauf des aktuellen Takts. Der Ring dreht sich genau einmal pro Takt (vier Schläge), die vier dicken Striche sind die Schläge, oben steht fest der Abspielpunkt, außen läuft der Fortschritt des ganzen Songs als Bogen. Die Drehung hängt an der Taktphase der Engine, nicht an einer Zeitanimation: Wer Deck A und Deck B nebeneinander sieht, sieht auch, wie weit beide auseinanderliegen.
2. **Zwei Farben für zwei Decks, sonst nichts.** Deck A Knochenweiß, Deck B Signalorange. Kein Cyan-Pink-Paar, keine Neon-Palette. Orange heißt außerdem "läuft jetzt" und "laut" (Kick-Pads, Läuft-Kachel).
3. **Die Kerbe.** Jede Kachel hat oben rechts eine abgeschnittene Ecke (14 dp, 45°), wie die Frontplatte von Hardware. Schalter, Pads und Zeilen sind dagegen fast eckig (3 dp). Das ist das Wiedererkennungsmerkmal im Standbild.
4. **Das Tonartenrad.** In der Bibliothek zeigt ein echtes Camelot-Rad (12 Felder außen Dur, innen Moll), welche Tonarten zum gewählten Song passen.
5. **Zahlen als Plakat.** BPM und Restzeit stehen in breiter, fetter Schrift (Archivo Expanded) groß im Bild. Der Rest ist ruhige Archivo.
6. **Der Takt zieht sich durch.** Beat-Punkte oben, Atmen von Kopf-Kacheln, Scheibe, Sequencer-Playhead und Schritt-Ring bei den Pads laufen auf einer Uhr.

Konkurrenztest: Das Pegel-Ring-in-Takt-Prinzip, die Kerbe und die Zwei-Farben-Regel hat keine der bekannten DJ-Oberflächen (Rekordbox, Traktor, djay sind schwarz mit bunten Wellenformen und rechteckigen Panels).

## 3. Tokens

Kotlin-Vorlage: [tokens/AutoDjTokens.kt](tokens/AutoDjTokens.kt). Rollen bleiben **namensgleich zur ersten Fassung** (`Ds.c.tile`, `Ds.c.ink` …), damit der Umbau im Code vor allem Werte und Formen ändert, nicht jede Stelle.

### 3.1 Farben (eine Variante: dunkel)

| Rolle | Wert | Einsatz |
| --- | --- | --- |
| ground | #0A0A0C | Fenster |
| tile | #131316 | Kachel |
| tileSunk (neu) | #1B1B20 | vertiefte Fläche (Scheiben-Bühne, Segment-Hintergrund) |
| tile2 | #26262C | Button ruhend, Fader-Spur, Zellen |
| tile3 | #34343C | Hover, aufgeklappte Zeile, Wertbogen-Spur |
| ink | #F1EEE6 | Text, "gewählt", globale Aktion (Play, Hauptbutton) |
| ink2 | #9B9BA5 | Zweittext (6.5:1 auf tile) |
| onInk | #0A0A0C | Text auf ink, orange und Pads |
| screen | #050507 | Displays (Wellenformen, Sequencer, Zeitleiste, Granular) |
| screen2 / screen3 | #1B1B20 / #26262C | Sequencer-Zellen, Beat-Spalten |
| screenBand (neu) | #0E0E12 | Taktbänder in Wellenformen |
| accent = ink | #F1EEE6 | Alias, bis alle Stellen auf `ink` umgestellt sind |
| deckA | #F1EEE6 | Deck A: Buchstabe, Ring, Bogen, Regler |
| deckB / signal | #FF5A1F | Deck B und "läuft/laut" |
| good / warn / bad | #8FE388 / #FFC857 / #E5484D | Status, Kill. Nur als kleine Quadrate oder Tags |

Pad-Gruppen (Fläche = Licht, alles auf schwarzem Text): Kick #FF5A1F, Snare/Clap #F1EEE6, Hi-Hats/Shaker #8A97A6, Perkussion #A8C23A, FX #FFC857. Gedrückt: Helligkeit +18 %, Skala 0.965.

Camelot-Chips (Datenfarbe): `hsl((n−1)·30+200, Dur 55 % / Moll 70 %, Dur 74 % / Moll 64 %)`, Text immer #0A0A0C.

Regeln: Verläufe und Glühen gibt es nicht. Orange nie als Fließtextfarbe. Auf einer Fläche höchstens Knochen und Orange gleichzeitig plus Neutrales (Ausnahme Pads und Tonartenrad, das sind Datenfarben). Die helle Tag-Variante und `Ds.night` entfallen; wer später ein Hell will, leitet es aus diesen Rollen ab.

### 3.2 Schrift: Archivo (SIL OFL)

Dateien in [fonts/](fonts/): `Archivo-Regular.ttf` (400), `Archivo-Medium.ttf` (500), `Archivo-Expanded-Bold.ttf` (700, Breite "expanded"). Die Hanken-Dateien aus der ersten Fassung können entfallen.

| Stufe | Schnitt | Größe (Telefon) | Zeile | Spur | Einsatz |
| --- | --- | --- | --- | --- | --- |
| display | Expanded Bold | 56 sp (44) | 0.95 | −0.02 em | **nur Zahlen**: BPM an der Scheibe, Restzeit, große Kopfzahl |
| head | Expanded Bold | 28 sp (24) | 1.0 | −0.015 em | Songname am Deck, Titel von Effekt-Kacheln, "Songs hinzufügen" |
| lead | Archivo Medium | 18 sp | 1.2 | 0 | Kachel-Untertitel, Listennamen, Segment-Titel |
| body | Archivo Regular / Medium | 15 sp | 1.4 | 0 | Buttons, Werte |
| small | Archivo Regular | 13 sp | 1.4 | 0 | Zweittext. Nie kleiner |

Zusätzlich: BPM in Listen und Mix-Zeilen in Expanded Bold 20 sp. Zahlen immer tabellarisch (`fontFeatureSettings = "tnum"`). Keine Großbuchstaben-Etiketten und keine Monospace (auch wenn sie nach DJ-Gerät aussehen: sie lesen sich auf dem Telefon schlecht und wirken wie Kostüm). Die Marke "AUTO·DJ" ist Expanded Bold 17 sp mit orangem Punkt.

### 3.3 Form, Raster, Abstand

- **Kachel:** `CutCornerShape(topEnd = 14.dp)`, Telefon 10 dp. Flache Kacheln (Displays, Sequencer, Zeitleiste) und alles Bedienbare: `RoundedCornerShape(3.dp)`. Knöpfe sind Kreise.
- Fensterrand 12 dp (Telefon 8), Abstand 8 dp (Telefon 6), Kachel-Innenabstand 16 dp (Telefon 12). 12-Spalten-Raster.
- Höhen: Leiste 56, Button 44 (klein 34), Zeile 64, Segment-Eintrag 34, Tippfläche mindestens 44.
- Keine Linien, Rahmen, Schatten, Glas. Trennung durch Tonstufe (tile → tileSunk → tile2 → tile3).
- Jeder Tab füllt das Fenster (1180×800 und 1440×900); höchstens eine ruhige Fläche.

### 3.4 Bewegung

Wie in der ersten Fassung: easeOut `cubic(0.2, 0, 0, 1)`; instant 90 ms (Druck), fast 160 ms (Ton, Toggle, Knopf), base 260 ms (Tab-Block, Zeile klappt auf), enter 320 ms mit 45 ms Staffel (höchstens 8 Stufen, 16 dp Aufstieg). Nur Transparenz, Versatz, Skalierung und Drehung. "Bewegung reduzieren": keine Drehung der Scheibe (stattdessen steht der Ring, nur die vier Schlagstriche des aktuellen Schlags leuchten), kein Puls, keine Wege.

## 4. Bausteine

Namen wie im Code (`design/Components.kt`); unverändert in der Signatur, geändert in Aussehen, wo angegeben.

| Baustein | Aussehen in v2 |
| --- | --- |
| `Tile` | tile, Kerbe oben rechts. Varianten: `pulse` (atmet), `sunk`, `flat` (3 dp, ohne Kerbe) |
| `Display` | screen, 3 dp, randlos |
| `Btn` | tile2, 3 dp, Text ink 500. `on`/`primary`: Füllung in Kontextfarbe (Deck A knochen, Deck B orange, sonst knochen), Text onInk |
| `Segment` | tileSunk-Block mit 2 dp Innenabstand, gewählter Eintrag in Kontextfarbe. Das Gleiten (SlidingChoice) bleibt |
| `NavTabs` | Kachel, Tabs 15 sp 500, gewählt = knochen Block mit onInk-Text, fährt in 260 ms |
| `BeatDots` | vier 14-dp-Quadrate (eckig). Aktueller Schlag knochen, der erste im Takt orange |
| `KeyChip` | 2 dp Radius, Camelot-Farbe |
| `StatusTag` | 2 dp, good/warn/bad als Fläche mit schwarzem Text |
| `BarFader` | tile2-Spur 28 dp, Füllung ink; bipolar ab Mitte mit 4-dp-Marke |
| `CapFader` | Spur 36 dp breit, Füllung der Kontextfarbe mit 35 % Deckkraft bis zur Kappe, Kappe ink 16 dp mit schwarzer Mittelmarke. Kill: Füllung bad |
| Crossfader | Spur 40 dp, Kappe 56 dp ink mit Mittelmarke |
| `LevelStrip` | **neu segmentiert**: 12 Segmente, 8 dp breit, 2 dp Abstand. 8 grün, 2 gelb, 2 rot, linear (kein Easing) |
| `Pad` | Gruppenfarbe, Name 17 sp Medium oben links, Taste 13 sp oben rechts |
| `HoldSurface` | große Gruppenfarb-Fläche, Titel unten links in head (24 sp) |
| `ProgressBar` | 28 dp, tile2-Spur, Füllung ink |
| `TileTitle` | head oder lead + Satz in small |
| **`Knob` (neu)** | Scheibe tile2 (Ø 40–96 dp), Kerbe ink, **Wertbogen** (3 dp, 270°) in Kontextfarbe außen, Spur tile3. Wert darunter 13 sp. Ersetzt Slider für alles Kontinuierliche außer Tempo, Level, Crossfader |
| **`Platter` (neu)** | siehe §5 |
| **`KeyWheel` (neu)** | siehe §7.1 |
| **`StepRing` (neu)** | 16 Quadrate (5 dp) im Kreis Ø 44; aktueller Schritt ink, Viertel ink2, Rest tile3. Zeigt bei den Pads den Sequencer-Schritt |

## 5. Die Taktscheibe (Platter)

Größe: `clamp(220 dp, 36 % der Fensterhöhe, 400 dp)`, auf dem Telefon höchstens Breite − 96 dp. Ab 1180×800 immer mindestens 250 dp.

Aufbau von außen nach innen:

1. **Fortschrittsbogen** am Rand: 4 dp, Spur tile3, Füllung Deck-Farbe, Start oben.
2. **Abspielpunkt**: Dreieck oben, ink, steht fest.
3. **Ring**: Dicke 15 % des Durchmessers, 180 Striche. Strichlänge = Pegel (Spitzenwert) der jeweiligen 1/180 des aktuellen Takts; Deck-Farbe, 62 % Deckkraft, Strichbreite 2.4 dp. Die vier Schlagstriche (jeder 45.) sind 5 dp dick und voll deckend. **Winkel = 360° · (beatIndex + beatPhase) / 4** (aus `Beat`/`DjStatus`, nie aus einer eigenen Uhr). Beim Taktwechsel wird der Ring mit dem Pegelverlauf des neuen Takts neu gezeichnet; bei Pause steht er.
4. **Nabe**: 60 % des Durchmessers, Kreis in tileSunk mit Puls (bis 8 % Richtung ink). Darin BPM in display, darunter "BPM" in small.
5. Daneben (Deck A rechts, Deck B links gespiegelt): **Tempo-Fader** vertikal, 36×230 dp, Wert und "Tempo" darunter in small.

Datenbedarf für den Code: Pegelverlauf je Takt aus der vorhandenen Analyse (Beat-Raster liegt in `DjStatus`/Analyse vor; steht er nicht pro Takt bereit, reicht als erste Stufe die gefaltete Gesamthüllkurve von 180 Werten).

## 6. Aufbau je Tab (Fenster 1180–1440 × 800–900)

Leiste oben (56 dp): Marke, Tabs, Beat-Punkte, BPM-Kachel (BPM in head 30 sp mit "BPM / freier Takt"), Play (knochen).

### 6.1 Decks ([Bild 1440](screens/decks_1440.png), [1180](screens/decks_1180.png))
Drei Spalten: Deck A | Mixer 264 dp | Deck B. Darunter über die ganze Breite der **Überblick**: zwei Spuren (A knochen, B orange), gespielter Teil voll, Rest 40 %, Taktbänder abwechselnd screen/screenBand, Playhead ink; er füllt die Resthöhe.
- Deck-Spalte: Kopf-Kachel (`pulse`): Buchstabe als 56-dp-Fläche in Deck-Farbe (Text onInk), Songname head, Zeit small, Tonart-Chip. Darunter die Scheibe mit Tempo-Fader. Darunter die Zeile: **Pause/Play** (Deck-Farbe, 52 dp, breit), Cue, Sync, Key (Sync/Key an = Deck-Farbe).
- Mixer: zwei Kanalstreifen A und B. Je Streifen: drei EQ-Knöpfe (High, Mid, Low) mit "Kill" daneben, ein Filter-Knopf (LP · HP), darunter Pegelanzeige (12 Segmente) und Level-Fader. Darunter der Crossfader mit Kurven-Segment (Weich, Gleich, Schnitt).
- Telefon: oben Segment "Deck A | Deck B", es steht jeweils eine Deck-Spalte; Mixer und Überblick folgen darunter.

### 6.2 Bibliothek ([Bild](screens/bibliothek_1440.png))
Links 8 Spalten: Zeilen 64 dp: Name (lead), **Mini-Wellenform** 150 dp, BPM (Expanded Bold 20), Tonart-Chip, Länge. Die gewählte Zeile (tile3) klappt auf: große Ladeflächen **A** (knochen) und **B** (orange) 64×48 dp, "In die Mix-Liste", "Entfernen". Rechts 4 Spalten: "Songs hinzufügen" (knochen Fläche, schwarzer Button), Tonartenrad (§7.1), Statistik (große Zahl).
Leer-Zustand: die Liste ist eine große Kachel "Noch keine Songs" mit Aktion "Songs hinzufügen".

### 6.3 Auto-Mix ([Bild](screens/auto-mix_1440.png))
Oben die **Zeitleiste** über die ganze Breite (Display, 170 dp): zwei Spuren, Songs wechseln zwischen A (knochen) und B (orange) ab; die **Überblendungen** liegen als hellere Bänder über beiden Spuren mit dem Namen ("Bass-Tausch"); der Abspielpunkt ist ein ink-Strich.
Darunter links 7 Spalten: Mix-Liste. Song-Zeile (der laufende ist knochen mit orangem Quadrat), dazwischen die Übergangszeile (44 dp, eingerückt 20 dp, tileSunk): Qualitätsquadrat (grün passt, gelb weich), Typ, Beats, BPM-Wechsel, Erklärung in small; läuft der Übergang, füllt er sich linear. Rechts 5 Spalten: **Läuft-Kachel in Orange** (Songname head, Restzeit display 48, Fortschrittsbalken, "Stopp", "Nächster Song", "Übergang in …"), Übergänge (Segmente Typ, Länge, Reihenfolge), Export.

### 6.4 Effekte ([Bild](screens/effekte_1440.png))
Wie ein Effekt-Rack in Streifen. Oben **Echo** über die ganze Breite (Schalter als 44-dp-Fläche, Titel head, Satz, Segment Abstand, zwei Knöpfe Wiederholung/Pegel Ø 60). Zweite Zeile: **Hall** und **Flanger** je 6 Spalten mit einem Knopf Ø 60. Dritte Zeile füllt den Rest: **Beat Roll** (FX-Gelb), **Stutter** (Orange), **Granular-Freeze** mit Aufnahme-Display; die Halten-Flächen sind groß, Titel unten links.

### 6.5 Pads ([Bild](screens/pads_1440.png))
Leiste: **Schritt-Ring**, Segment Muster (House, Techno, Hip-Hop, Breakbeat, Leer), Knöpfe Lautstärke/Swing (Ø 44 mit Wert), "Läuft". Darunter links 5 Spalten die 4×4 **Pads** (füllen die Höhe), rechts 7 Spalten der **Sequencer** als Display: 16 Zeilen × 16 Schritte, Beat-Gruppen als Tonbänder (screen2/screen3), aktive Schritte in Gruppenfarbe, Playhead-Spalte aufgehellt, Kopfzeile 1 bis 4.

### 6.6 Granular ([Bild](screens/granular_1440.png))
Gleiches Material: links das Display (Wolke und Wellenform, Körner knochen mit orangen Akzenten), rechts Steuerkachel und Hüllkurve (füllt den Rest), unten die Knöpfe space, chaos, length, **grains** (Ø 88, größter), position, tune, output. Beschriftungen bleiben klein geschrieben wie bisher.

### 6.7 Telefon ([alle Tabs](screens/telefon_alle_tabs_390.png))
Leiste: Marke, BPM, Play. Unten die Navigation als Kachel (Songs, Mix, Decks, FX, Pads, Granular, 13 sp). Alles einspaltig. Songzeilen ohne Mini-Wellenform und Länge, Übergangszeilen ohne Erklärung, Sequencer ohne Zeilennamen, Granular-Zellen 4 × 4.

## 7. Weitere Bausteine im Detail

### 7.1 Tonartenrad
Ø ≥ 220 dp (füllt die Kachel). 12 Sektoren, außen Dur (B, 126→88), innen Moll (A, 84→46), Zahl 13 sp; 12 steht oben. Das gewählte Feld ist knochen, passende (gleiche Zahl, ±1, A/B-Wechsel) in ihrer Camelot-Farbe, der Rest tile2 mit ink2-Zahl. In der Mitte die Tonart in Expanded Bold 20 und "Moll"/"Dur". Tippen auf ein passendes Feld filtert die Liste (Vorschlag, nicht im Code).

### 7.2 Puls
Wie in der ersten Fassung: `pulse = (1 − phase)³`, erster Schlag voll, sonst 55 %, aus bei Pause und "Bewegung reduzieren", nur in Draw-Blöcken lesen. Wirkt auf: BeatDots, `Tile.pulse` (Kopf-Kachel, BPM-Kachel, bis 7 % Richtung ink), Nabe der Scheibe (8 %), Sequencer-Playhead, Schritt-Ring. **Die Drehung der Scheibe folgt `beatIndex + beatPhase` direkt, ohne Interpolation.**

## 8. Umsetzung (Vorschlag in Reihenfolge)

1. `design/Design.kt`: Farben (§3.1), Rollen `tileSunk`, `screenBand` neu, `Ds.night` und Tag-Variante entfernen, Schrift auf Archivo (`fonts/` einbinden), Formen (Kerbe/3 dp).
2. `Components.kt`: Tile (Kerbe), Btn/Segment/NavTabs/Pad (Form und Farbe), LevelStrip segmentiert, Kontextfarbe (`LocalDeckColor`) für Deck A/B einführen.
3. Neue Bausteine: `Knob`, `Platter`, `StepRing`, `KeyWheel`, Überblick-Zeile (`Overview`).
4. Tabs: Decks (Platter + Mixer), Bibliothek (Mini-Wellen, Rad), Mix (Zeitleiste oben), Effekte (Rack), Pads (Ring), Granular.
5. Puls und Einblenden bleiben wie bisher; Scheibe an `Beat` hängen.

Screenshot-Test: die Bilder in `screens/` sind das Soll (Desktop-Test `:desktopApp:test` rendert dagegen).

## 9. Offen und Annahmen

- Beispielinhalte (Songnamen, Zeiten) sind erfunden.
- Tonartenrad-Filter, Pad-Tasten und der Pegelverlauf je Takt sind Vorschläge; Datenbasis siehe §5.
- Die Kerbe und die Scheibe sind aufwendiger als Standardformen: `CutCornerShape` (Compose) und ein eigener Canvas. Beides ist in Compose Desktop und Android möglich; nicht auf echter Hardware gesehen.
- Orange hat zwei Bedeutungen (Deck B und "läuft/laut"). Bewusst so gewählt, damit die Palette bei zwei Farben bleibt; falls es in der Praxis verwechselt wird, bekommt "läuft" eine eigene Form (Quadrat statt Fläche).
- Die Hanken-Grotesk-Schrift und die helle Fassung liegen unter `archiv/v1-knochen/` nur noch als Referenz.
