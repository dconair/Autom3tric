# Auto DJ: Designsprache "Kabine" (Version 2.1: mit wählbaren Looks, Farbschemata und freier Anordnung, siehe §10)

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

Regeln: Verläufe und Glühen gibt es nicht. Orange nie als Fließtextfarbe. Auf einer Fläche höchstens Knochen und Orange gleichzeitig plus Neutrales (Ausnahme Pads und Tonartenrad, das sind Datenfarben). Die helle Tag-Variante und `Ds.night` entfallen; wer ein Hell will, nimmt das Schema „Papier“ (§10.4).

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

## 10. Looks, Farbschemata und freie Anordnung (neu in 2.1)

Wunsch des Nutzers: mehrere Designs zur Auswahl, mit Farbschemata, und die Elemente selbst platzieren können. Der Prototyp ([mockups/prototype.html](mockups/prototype.html), Zahnrad-Symbol oben rechts oder `#look`) zeigt alles live. Die Kabine aus §2 bis §9 bleibt der Standard und der Ausgangspunkt für alle weiteren Looks.

### 10.1 Was die Recherche ergab

Quellen waren Websuchen, keine eigenen Tests der Programme: Eindrücke, keine Messwerte.

- **VirtualDJ** gilt als die am stärksten anpassbare Software: Layouts (Starter, Essentials, Pro, Performance) plus herunterladbare Skins ([DJ.Studio-Leitfaden](https://dj.studio/blog/ux-buyers-guide-mixing-software)). Serato, Traktor und rekordbox bieten kaum Skins ([Forum](https://community.enginedj.com/t/lord-have-mercy-serato-acquired-by-alphatheta-corporation/49775?page=19), Nutzeraussage).
- **djay** hat feste Optik, aber Ansichten: Wellen waagerecht oder senkrecht, Plattenteller ein oder aus ([Algoriddim-Handbuch](https://help.algoriddim.com/user-manual/djay-pro-windows/getting-started/decks)). Nutzer wünschen sich mehr Platz-Kontrolle, etwa die Bibliothek neben allen Reglern ([Forum](https://community.algoriddim.com/t/allow-library-visibility-with-all-essential-dj-controls/42498)).
- **Serato** erlaubt Wellen waagerecht oder senkrecht, zwei oder vier Decks und anordenbare Bereiche (Prepare, History, Browse); **rekordbox** trennt einen Vorbereitungs- und einen Auftrittsmodus.
- **Wellenfarben:** rekordbox kennt Blau, RGB und 3 Bänder; Serato färbt nach Frequenz (Rot Bass, Grün Mitten, Blau Höhen); Traktor reicht von einfarbig (X-Ray) bis Vollspektrum ([Digital DJ Tips](https://www.digitaldjtips.com/?p=2475091), [Elephorm](https://www.elephorm.com/formation/maitriser-rekordbox-6/comprendre-les-differents-modes-daffichage-de-la-waveform-dans-rekordbox-6)).
- **Ableton-Themes** (von der Community) sind nach Kontrast, Palette und Helligkeit gruppiert: hell, mittel, dunkel, ultradunkel ([CDM](https://cdm.link/bitwig-themes/)). Daraus die Idee, Schemata über die Helligkeit klar zu unterscheiden.

Folgerung: Anpassung hat drei Ebenen, und die Programme wählen jeweils nur eine. Wir bieten alle drei unabhängig an: **Look** (Form, Dichte, Schrift, Aufbau des Decks), **Farbschema** (Palette) und **Anordnung** (Module auf einem Raster). Dazu kommen die Wellenform (Stil, Ansicht).

### 10.2 Ein Schalter je Ebene, alles über Rollen

Weil Komponenten nur Rollen kennen (§3), kostet jede Kombination nichts extra: 3 Looks × 6 Schemata × 3 Wellenstile × 3 Deck-Ansichten = 162 Zustände ohne eine Zeile Sonderlogik. Einstellungen leben in einem `AppearancePrefs` und gelten sofort, ohne Neustart.

### 10.3 Looks

| Wert | Kabine | Pult | Bühne |
| --- | --- | --- | --- |
| Charakter | Taktscheibe, Kerbe, breite Zahlen | Eckig, dicht, Welle zuerst | Rund, groß, wenig auf einmal |
| Kachel-Kerbe | 14 dp | 0 | 0 |
| Radius Bedienelemente | 3 dp | 0 | 14 dp |
| Radius Kachel | 0 | 0 | 26 dp |
| Abstand / Fensterrand | 8 / 12 dp | 3 / 6 dp | 12 / 16 dp |
| Kachel-Innenabstand | 16 dp | 12 dp | 20 dp |
| Bedienhöhe (Button, Tab) | 44 dp | 40 dp | 52 dp |
| Zahlen und Überschriften | Archivo Expanded Bold | Archivo Medium | Archivo Medium |
| Standard-Deck-Ansicht | Scheibe | Welle | Scheibe |

Alles andere (Palette, Bewegung, Komponentenaufbau) ist gleich. Das Telefon nimmt dieselben Looks, mit den Abständen aus §3.3 (Pult und Bühne skalieren proportional). Ein Look ist im Code ein `AutoDjLook` (siehe [tokens/AutoDjTokens.kt](tokens/AutoDjTokens.kt)) in einem `CompositionLocal`.

### 10.4 Farbschemata

Jedes Schema definiert den vollständigen Rollensatz aus §3.1 (Flächen, Tinte, Deck A/B, Pad-Gruppen, Status, Display, Wellenbänder). Displays (Wellen, Sequencer, Zeitleiste) bleiben in jedem Schema dunkel, auch im hellen. Orange/Deck B heißt immer „läuft“ und „laut“.

| Schema | Fläche (Fenster / Kachel) | Tinte | Deck A | Deck B | Beschreibung |
| --- | --- | --- | --- | --- | --- |
| Kabine | #0A0A0C / #131316 | #F1EEE6 | #F1EEE6 | #FF5A1F | Knochen und Signalorange (Standard) |
| Eis | #05080D / #0C131C | #E8F2FF | #4DB8FF | #5CF2C0 | Eisblau auf Nachtblau, nah an Pioneer-Blau |
| Amber | #0B0804 / #15100A | #FFE9C7 | #FFB000 | #FF6A3D | Bernstein wie ein Hardware-Display |
| Neon | #0B0912 / #14111F | #F4F0FF | #C6FF3D | #FF3DAE | Limette und Magenta für den Club |
| Papier | #E4E0D6 / #F3F0E9 | #1A1714 | #2F5FE0 | #C0390E | Hell, für Tageslicht |
| Mono | #0C0C0C / #161616 | #FFFFFF | #FFFFFF | #FFD400 | Weiß mit einem einzigen Gelb |

Alle Werte stehen im Prototyp (`[data-scheme=…]`) und in `AutoDjTokens.kt`. Gemessener Kontrast (WCAG): Zweittext auf Kachel 6,3 bis 7,4 in allen Schemata, Haupttext über 15; Text auf Deck-Farbe und Pads mindestens 4,9 (Papier: Deck A 4,9, Deck B 4,9). Gering ist nur der Hell-Text auf dem Hi-Hat-Pad in Eis und Mono (5,9 bis 6,2, genügt). Camelot-Chips behalten ihre Datenfarben in jedem Schema.

Regeln für neue Schemata: eine Tintenfarbe, zwei Deck-Farben, die sich auf dem Display unterscheiden (Hell/Dunkel oder Farbton), Pad-Gruppen aus Deck-Farben plus Neutralem, Kontrast wie oben. Schemata nach Helligkeit sortieren (Hell, Mittel, Dunkel), nicht nach Name.

### 10.5 Wellenform-Stil und Deck-Ansicht

- **Stil** (gilt für Überblick, Deck-Welle, Zeitleiste): *Einfarbig* (Deckfarbe), *3 Bänder* (Bass, Mitten, Höhen als gestapelte Schichten in `w1`, `w2`, `w3` des Schemas) und *Hitze* (je lauter, desto heller, Deckfarbe zu Tinte). Der Stil ändert nur die Füllung der Striche, nie die Geometrie.
- **Deck-Ansicht:** *Scheibe* (Taktscheibe aus §5), *Welle* (laufende Welle mit Beat-Raster, Abspielstelle in der Mitte, Taktstriche oben und unten) oder *Beides* (Welle 90 dp über kleinerer Scheibe). Die Welle läuft aus derselben Takt-Phase wie die Scheibe: Versatz = Zeit × BPM/60 × 64 dp je Schlag, ein Takt = vier Schläge, die vier Schlagstriche des aktuellen Takts voll deckend.
- Die Bibliothek-Miniaturen bleiben einfarbig (ink2), damit die Liste ruhig bleibt.

### 10.6 Anordnung: Module auf einem 12 × 8 Raster

Jede Seite (Tab) besteht aus **Modulen**. Ein Modul ist eine abgeschlossene Kachelgruppe, die sich an die Größe anpasst, die ihr das Raster gibt, nicht an die Fenstergröße (im Prototyp mit Container-Größen; in Compose mit `BoxWithConstraints`). Beispiel Scheibe: Durchmesser = `min(Modulhöhe − 235 dp, Modulbreite − 130 dp)`, mindestens 110 dp, höchstens 420 dp.

**Module der Deck-Seite** (Prototyp, Mindestgröße in Feldern, Spalten × Zeilen):

| Modul | Mindestens | Standardgröße |
| --- | --- | --- |
| Deck A, Deck B | 4 × 4 | 5 × 6 |
| Mixer | 2 × 5 | 2 × 6 |
| Überblick (zwei Wellen) | 3 × 1 | 12 × 2 |
| Bibliothek (Liste mit A/B) | 3 × 3 | 4 × 6 |
| Effekte (Kurzfassung) | 3 × 3 | 4 × 4 |
| Pads (4 × 4) | 3 × 3 | 4 × 5 |

**Voreinstellungen:** *Standard* (A, Mixer, B, Überblick), *Deck-Fokus* (Decks über die ganze Höhe, kein Überblick), *Bibliothek* (Liste links, Decks gestapelt, Mixer rechts), *Pads + Effekte* (kleine Decks, großer Überblick, Pads und Effekte rechts). Wer etwas ändert, landet in *Eigene*.

**Bearbeiten** (nur Rechner, Taste „Anordnen“ in der Leiste über dem Raster): Das Raster wird sichtbar, die Inhalte treten zurück, jedes Modul zeigt Name, Größe, ein × und einen Griff rechts unten.
- **Ziehen** an beliebiger Stelle verschiebt, gerastet auf Felder. **Griff** ändert die Größe, nie unter die Mindestgröße.
- **Zusammenstoß:** das bewegte Modul behält seinen Platz, getroffene Module rutschen nach unten. Passt dadurch etwas nicht mehr ins Raster, wird die Bewegung nicht ausgeführt (kein Überlauf, keine Rollleiste).
- **× blendet aus**, die Leiste bietet „+ Name“ zum Einblenden. Findet sich kein Platz, versucht die App kleinere Größe und Verdrängung, sonst Hinweis „Kein Platz: erst ein anderes Modul verkleinern oder ausblenden“.
- Andere Module gleiten in 220 ms (easeOut) an ihren neuen Platz. „Zurücksetzen“ stellt *Standard* her.

**Telefon:** keine freie Platzierung (zu klein). Stattdessen in „Aussehen › Anordnung“: Module ein- und ausblenden und mit ▲▼ die Reihenfolge ändern; die Seite zeigt sie untereinander.

**Speicherung:** pro Gerät, sofort. `AppearancePrefs(look, scheme, wave, view, order: List<ModuleId>, layouts: Map<TabId, LayoutSpec>)`, mit `LayoutSpec(preset: String, modules: Map<ModuleId, Cell(x, y, w, h)>)`. Der Prototyp legt dasselbe unter `localStorage` ab. Aus dem Raster abgeleitet, nie aus Pixeln, damit es zwischen 1180 × 800 und 1440 × 900 und beim Drehen gleich bleibt.

### 10.7 Seite „Aussehen“ ([Bild](screens/aussehen_1440.png))

Erreichbar über das Zahnrad in der Leiste (Telefon: oben rechts, neben Play). Von oben nach unten: **Look** (drei Karten mit Live-Vorschau des Decks, die gewählte in `ink`), **Farbschema** (sechs Karten mit Farbstreifen) neben **Wellenform** (Stil und Deck-Ansicht, Vorschau), **Anordnung** (Module ein/aus, „Zurücksetzen“, „Anordnen …“, springt zu den Decks im Bearbeiten-Modus). Änderungen gelten sofort.

Bilder: [Looks und Schemata im Überblick](screens/schemata_1440.png), [Pult + Eis + 3 Bänder](screens/decks_pult_eis_1440.png), [Bühne + Amber](screens/decks_buehne_amber_1440.png), [Papier, Scheibe und Welle, Hitze](screens/decks_papier_1440.png), [Bearbeiten](screens/anordnen_1440.png), [Bibliothek-Anordnung](screens/layout_bibliothek_1440.png), [Pads-Anordnung](screens/layout_pads_1440.png), [Telefon](screens/telefon_looks_390.png).

### 10.8 Umsetzung (Vorschlag in Reihenfolge, für den Code-Thread)

1. `AutoDjColors` gibt es sechsmal (Werte aus `AutoDjTokens.kt`); `LocalAutoDjColors` wechselt per `AppearancePrefs`. Das bestehende `Ds.night` fällt weg (Schema „Papier“ ersetzt das helle Thema).
2. `AutoDjLook` (Kerbe, Radien, Abstände, Bedienhöhe, Display-Schrift, Standardansicht) als `CompositionLocal`. Komponenten lesen daraus statt fester `dp`-Werte (Kerbe/Radius, `AutoDjSpace`, Höhen).
3. Seite „Aussehen“ plus `AppearancePrefs` (DataStore).
4. `WaveformView(style, scrolling)` mit den drei Stilen; Deck bekommt die Ansicht Scheibe/Welle/Beides.
5. Modul-Raster für die Deck-Seite: `ModuleGrid(layout, editing)` als eigenes `Layout` (12 × 8), Module als `BoxWithConstraints`. Danach dasselbe für Bibliothek, Effekte und Pads (dann Module wie „Pad-Raster“, „Sequencer“ einzeln).
6. Bearbeiten-Modus mit `detectDragGestures` (Ziehen, Größe) und der Zusammenstoß-Logik aus dem Prototyp (`resolve()`).

### 10.9 Annahmen und offen

- Im Prototyp ist nur die **Deck-Seite** frei anordenbar; die anderen Tabs sind fest. Die Module dafür wären: Bibliothek (Liste, Tonartenrad, „Songs hinzufügen“), Auto-Mix (Zeitleiste, Liste, Übergänge, Export), Effekte (je Effekt eine Kachel), Pads (Pad-Raster, Sequencer), Granular (Wolke, Schritte, Hüllkurve, Regler).
- Looks sind bewusst nur drei. Weitere (etwa ein heller Apple-artiger) sind über `AutoDjLook` später billig; sie sollen sich in Form und Aufbau unterscheiden, nicht nur in der Farbe.
- Pult und Bühne wurden nur auf Desktop-Auflösungen und 390 dp Breite angeschaut. Nichts davon lief auf echter Hardware.
- Die Beschreibungen der Referenz-Software stammen aus Suchergebnissen, die teils Foren und Werbetexte sind; sie belegen die Richtung, nicht die Details.
