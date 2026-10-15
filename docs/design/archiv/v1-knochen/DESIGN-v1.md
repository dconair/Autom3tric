# Auto DJ: Designsprache und Aufbau

Stand 2026-10-15. Diese Spezifikation gilt für die Desktop-App (macOS) und die Android-App. Sie ändert keinen Code; der Code-Thread setzt sie um. Sehen und anklicken: [mockups/prototype.html](mockups/prototype.html) (Datei im Browser öffnen, oben rechts Tag/Nacht, die Tabs sind klickbar, Pads und Halten-Flächen reagieren). Fertige Bilder liegen in [screens/](screens/).

Entstanden mit dem Skill design-taste-frontend. Der Skill ist für Webseiten gedacht; übernommen wurden seine Art Direction (Richtung, Idee, Typografie, Palette, Anti-Slop-Regeln) und der Blick-und-Korrigier-Ablauf, nicht Landingpage-Bausteine.

## 1. Was heute nach "KI-generiert" aussieht

Aus den Screenshots in `auto-dj-screens/`:

- Alles ist dieselbe Karte: gleicher dunkler Kasten, 1px-Rand, gleiche Rundung. Nichts ist wichtiger als etwas anderes.
- Fast aller Text ist 11 bis 12 sp, grau auf dunkel. Es gibt keine große Zahl, kein Bild, das man behält.
- Pillen überall (Auswahl, Tempo, Kurve, Muster), dazu Cyan und Pink als Deck-Farben, das Standard-Kostüm eines Dunkelmodus.
- Regler sind Balken mit einem weißen Strich und einem Punkt. Sie sehen nicht aus wie etwas, das man anfasst.
- Granular ist beige-Hardware, der Rest dunkles Dashboard: zwei verschiedene Apps.

## 2. Entscheidung

**Richtung: das gut gepflegte Board** (Skill-Richtung `dense-tiles`, Seed 911543420, keine Neuzüge). Das ganze Fenster ist ein lückenlos gefülltes Brett aus flachen Kacheln mit 8 dp Abstand. Jede Kachel enthält eine Sache. Keine Ränder, keine Schatten, keine Verläufe; Flächen trennen sich durch Tonstufen.

**Palette: Knochen, Grün, Schokolade** wie gezogen (Hardware-Instrument, nicht Neon-Dashboard). Das passt zum beigen Granular-Instrument und macht es zum Vorbild für alles andere. Dunkle "Displays" (Wellenform, Sequencer, Zeitleiste, Granular-Wolke) sind Kacheln in Schokolade-Schwarz auf dem hellen Brett. Eine Nacht-Variante für dunkle Räume gehört dazu (§3.1). Anpassung gegenüber dem Zug: dazugekommen sind Schiefer, Taupe und Sand als Flächenfarben für Deck B und die Pad-Gruppen, weil ein DJ-Werkzeug mehr als eine Unterscheidung braucht. Grün bleibt ausschließlich Aktion und "läuft".

**Schrift: Hanken Grotesk**, Regular und Medium, sonst nichts. Ein Typ, fünf Größen. Dateien liegen in [fonts/](fonts/) (SIL Open Font License, Quelle Google Fonts).

**Die Idee (Konkurrenztest bestanden):**

> Der Takt des laufenden Songs wird zum Puls der Oberfläche: Beat-Punkte, Deck-Kopf, Transport-BPM, Pads und Sequencer-Playhead laufen auf derselben Uhr.

Rekordbox, Traktor oder djay haben ein Beat-Gitter in der Wellenform, aber keine Oberfläche, die selbst im Takt atmet. Bei uns ist das Takt-Prinzip ohnehin die Architektur (Echo, Stutter, Sequencer, Übergänge rechnen in Beats), also zeigt auch die UI es. Dazu: Takte (Vierergruppen) werden auf den Displays als **Tonbänder** statt als Rasterlinien gezeigt.

**Das Live-Ding:** der Puls selbst, plus die Tonarten-Kachel in der Bibliothek ("Passende Tonarten": welche Camelot-Felder zum Song auf dem Deck passen, direkt anklickbar). Letzteres ist ein Vorschlag, noch nicht im Code (§9).

## 3. Tokens

Die Kotlin-Fassung steht in [tokens/AutoDjTokens.kt](tokens/AutoDjTokens.kt); Komponenten dürfen nur Rollen kennen, keine Hex-Werte.

### 3.1 Farbrollen

| Rolle | Tag | Nacht | Einsatz |
| --- | --- | --- | --- |
| ground | #E5E1D8 | #141010 | Fenster |
| tile | #DAD5CA | #1F1916 | jede Kachel |
| tile2 | #CFC9BC | #2B231E | Spur von Fadern, Segment-Hintergrund, Button ruhend |
| tile3 | #C3BDAF | #392E27 | Hover, aus-geschaltete Beat-Punkte |
| ink | #2A1D17 | #EDE7DB | Text, Drehknöpfe, "ausgewählt" |
| ink2 | #5C4D44 | #A09386 | Zweittext (4.9:1 auf tile2) |
| screen / 2 / 3 | #1A1310 / #251B16 / #34271F | #0B0807 / #17110E / #261D18 | Displays; 2 = Taktband, 3 = Beat-1-Spalte |
| screenInk / Dim | #EDE7DB / #9A8D82 | unverändert / #8C8076 | Text auf Displays |
| accent | #456F3A (Text weiß, 5.9:1) | #8FBF7A (Text #14100E) | Play, "An", Hauptbutton, läuft |
| accentLit | #9CCB86 | gleich | Akzent auf Displays |
| Deck A | Feld #3A2A22, Licht #E9DFC8 | gleich | Buchstabe, Wellenform |
| Deck B | Feld #3F5F7F, Licht #9DBEE0 | gleich | Buchstabe, Wellenform |
| warn / bad | #E0B25A / #B5483A | gleich | Status, Kill |

Pad-Gruppen (Fläche, Licht auf Display): Kick #3A2A22 / #E9DFC8; Snare und Clap #456F3A / #9CCB86; Hi-Hats und Shaker #3F5F7F / #9DBEE0; Perkussion #766652 / #C7B79F; FX #C9B58A / #E4CF9C.

Camelot-Chips bleiben Datenfarben: `hsl((n-1)*30+200, Moll 40% / Dur 34%, Moll 72% / Dur 80%)`, Text immer #2A1D17.

Regeln: eine Grundfläche, eine Tinte, ein Akzent. Auswahl heißt immer ink-Füllung. Grün heißt immer Aktion oder läuft, nie Deck-Identität. Verboten: Verläufe, Glow, Violett/Indigo, Cyan/Pink-Paar.

### 3.2 Schrift

| Stufe | Größe (Telefon) | Gewicht | Zeilenhöhe | Spur | Einsatz |
| --- | --- | --- | --- | --- | --- |
| display | 64 sp (52) | 500 | 0.9 | -0.04 em | BPM an den Decks, große Zahl im Mix |
| head | 36 sp (30) | 500 | 1.0 | -0.035 em | Transport-BPM, "Songs hinzufügen", Titel der laufenden Nummer |
| lead | 22 sp | 500 | 1.15 | -0.02 em | Kachel-Titel, Songname am Deck |
| body | 15 sp (Songzeilen 17, Medium) | 400 | 1.45 | -0.005 em | Buttons, Listen, Werte |
| small | 13 sp | 400 | 1.4 | 0 | Zweittext. **Nichts kleiner als 13 sp.** |

Zahlen immer tabellarisch (`fontFeatureSettings = "tnum"`). Keine Monospace-Schrift, keine Großbuchstaben-Etiketten, keine Nummerierungen (01, 02), keine Eyebrow-Labels über Überschriften. Satzschreibung. Zeilen in Kacheln linksbündig.

### 3.3 Raster, Abstand, Form

- Fensterrand 12 dp (Telefon 8), Abstand zwischen Kacheln 8 dp (Telefon 6), Zeilen in Listen 4 dp. Alles auf einem 12-Spalten-Raster; Kachelgrößen nur 3, 4, 5, 6, 7, 8, 12 Spalten.
- Kachel-Innenabstand 16 dp (Telefon 12), Text oben links.
- **Ein Radius: 6 dp** für Kacheln, Buttons, Pads, Zeilen. Knöpfe sind Kreise. Es gibt keine Pillen.
- Mindest-Tippfläche 44 dp; Standardhöhe Button 44, klein 36, Zeile 60, Leiste 56.
- Keine Linien, keine 1-dp-Ränder, keine Schatten, kein Glas. Trennen durch Tonstufe (tile → tile2 → tile3) oder Abstand.
- Lückenlos: Auf 1180×800 und 1440×900 füllt jeder Tab das Fenster. Höchstens eine bewusst ruhige Fläche pro Tab.

### 3.4 Bewegung

| Token | Wert | Einsatz |
| --- | --- | --- |
| easeOut | cubic(0.2, 0, 0, 1) | alles Sichtbare |
| instant | 90 ms | Druck |
| fast | 160 ms | Tonwechsel, Toggle, Knopfdrehung |
| base | 260 ms | Tab-Indikator, Zeile klappt auf |
| enter | 320 ms | Einblenden von Kacheln |
| stagger | 45 ms je Kachel, höchstens 8 Stufen | Tab-Wechsel |
| rise | 16 dp | Weg beim Einblenden |
| pressSpring | Dämpfung 0.7, Steifigkeit 800 | Pads, Buttons loslassen |

Nur Transparenz und Versatz/Skalierung animieren, nie Größe oder Layout von ganzen Flächen. Alle Bewegungen laufen auch ohne: Schalter "Bewegung reduzieren" in der App (Desktop) beziehungsweise Android-Animationsskala 0 → keine Wanderung, kein Puls, nur Tonwechsel in 0 ms.

## 4. Komponenten

Alle Zustände hängen an Rollen aus §3.1.

| Komponente | Aussehen | Zustände und Bewegung |
| --- | --- | --- |
| Kachel | flach, tile, 6 dp, 16 dp Innenabstand | Varianten: `accent` (grün), `ink`, `screen` (dunkles Display, Innenabstand 0, Inhalt randlos), `pulse` (atmet im Takt) |
| Button | 44 dp hoch, tile2, Text ink | Hover tile3. Druck: Skala 0.97 in 90 ms, loslassen Feder. `on` = ink-Füllung. `primary` = accent-Füllung. Es gibt höchstens einen primary pro Kachel |
| Segment | ein tile2-Block mit 2 dp Innenrand, Einträge 36 dp, gewählt = ink-Füllung | Wechsel: Füllung fährt in 160 ms zum neuen Eintrag. Ersetzt alle Pillen-Gruppen (Echo-Abstand, Kurve, Muster, Beat-Länge …) |
| Drehknopf | ink-Kreis, eine helle Kerbe, Wert darunter in small, Titel darüber oder darunter; Größen 40, 56, 72, 88/96 | Wert dreht -135° bis +135° in 160 ms. Das ist die **einzige** Art kontinuierlichen Reglers außer Fadern. Das Granular-Instrument hat ihn schon |
| Balken-Fader (waagrecht) | tile2-Spur 28 dp hoch, ink-Füllung, bei bipolar Füllung ab Mitte und 4-dp-Marke in der Mitte | Tempo, Filter-Alternative, Lautstärke, Swing. Ganze Höhe ist Zielfläche |
| Kappen-Fader | tile2-Spur, 40 dp breit, Kappe 18 dp hoch in ink | EQ High/Mid/Low, Level. Kill = Kappe wird bad-rot |
| Crossfader | Spur 40 dp hoch, Kappe 64 dp breit in ink | A/B-Seite wird 160 ms auf 55 % Deckkraft gedimmt, wenn sie weggefadet ist |
| Pegelanzeige | 8 dp breiter Streifen, accent-Füllung | folgt dem Pegel linear (kein Easing) |
| Chip (Tonart) | 24 dp hoch, 4 dp Radius, Camelot-Farbe | nur Daten |
| Status | 24 dp, Füllfarbe good/warn/bad/idle, 13 sp Text | Übergangs-Qualität, Importstatus |
| Zeile | 60 dp, tile, Name 17 sp Medium, Zahlen rechts tabellarisch | ausgewählt = ink-Füllung und klappt auf (260 ms), Rest rutscht nach unten |
| Pad | quadratnah, Gruppenfarbe, Name oben links 17 sp Medium, Taste oben rechts 13 sp | Druck: Helligkeit +35 % sofort, Skala 0.965, Abfall über 180 ms Feder |
| Halten-Fläche | große Gruppenfarb-Kachel, Titel unten links 28 sp | gedrückt wie Pad, solange gehalten |
| Display (screen) | Kachel #1A1310, keine Ränder | Wellenform, Zeitleiste, Sequencer, Granular-Wolke |
| Navigation oben (Desktop) | Kachel mit Texttabs 15 sp; aktive Seite ist ein ink-Block, der in 260 ms hinüberfährt | ersetzt die Unterstreichung. Rechts: Beat-Anzeige, BPM-Kachel, Play-Aktion |
| Navigation unten (Android) | Kachel mit sechs Texten 13 sp (Songs, Mix, Decks, FX, Pads, Granular), aktiver Eintrag ink-Block | keine Symbole, kein Material-NavigationBar |

**Beat-Anzeige:** vier 14-dp-Quadrate (3 dp Radius). Das des aktuellen Schlags ist grün, der erste Schlag im Takt ink. Das ist gleichzeitig Zähler und Puls-Anker.

## 5. Der Puls

`pulse(beatPhase, beatIndex)` liefert 0 bis 1: `(1 - phase)^3`, auf dem ersten Schlag volle Stärke, sonst 55 %. Quelle ist die Takt-Phase des Master-Takts aus dem Engine-Status (die UI rät nicht selbst). Sie wirkt auf wenigen Stellen, damit es Atem bleibt:

- Beat-Punkte (siehe oben).
- Kacheln mit Rolle `pulse` (Transport-BPM, Deck-Kopf): Tonstufe bis 6 % Richtung ink.
- Sequencer: die Playhead-Spalte steht auf der aktuellen 16tel, der Kopf (Zahlen 1 bis 4) färbt den aktuellen Schritt.
- Pegelstreifen, Wellenform-Playhead: folgen dem Audio, kein Puls.

Aus bei Pause und bei "Bewegung reduzieren". Der Puls darf nie Text unlesbar machen (nur Kachelton, nie Textfarbe) und nie mehr als eine Neuzeichnung pro Frame auslösen: eine `State<Float>`, gelesen nur in `drawBehind`/`graphicsLayer`, nicht in der Komposition.

## 6. Animationen (Übersicht)

| Ereignis | Bewegung |
| --- | --- |
| Tab-Wechsel | Indikator fährt 260 ms. Inhalt: Kacheln steigen 16 dp auf und werden sichtbar in 320 ms, gestaffelt 45 ms je Kachel (max. 8), nur beim ersten Erscheinen des Tabs |
| Zeile in Bibliothek auswählen | Auswahlzeile wird ink, klappt 260 ms auf; Inhalt (Abschnitte, Buttons) steigt 16 dp auf |
| Song laden | Wellenform zeichnet sich von links in 420 ms auf (Reveal per Clip, keine Skalierung) |
| Segment / Toggle | Füllung wechselt in 160 ms |
| Knopf | dreht in 160 ms zum neuen Wert |
| Pad / Halten | Druck sofort, Loslassen Feder |
| BPM-Zahl ändert sich (Sync, Tempo) | Ziffern wechseln per Überblendung 160 ms |
| Übergang läuft (Mix) | Übergangszeile füllt sich linear von links mit tile3 nach Fortschritt |
| Crossfader | Gegenseite dimmt 160 ms |
| Reduzieren | alle Wege entfallen, Tonwechsel sofort |

Es gibt bewusst keine Dauer-Schleifen, kein Funkeln, keine Wellen.

## 7. Aufbau je Tab

Fenster Desktop: Mindestgröße 1100×720, Standard 1180×800. Alle Tabs: oben die Leiste (56 dp): Wortmarke "Auto DJ" in einer Kachel, Tab-Kachel, rechts Beat-Anzeige, BPM-Kachel (`head`-Zahl und "BPM / freier Takt" oder Songname), Play-Aktion. Danach das Brett mit 12 Spalten. Bilder: [screens/](screens/).

### 7.1 Bibliothek ([Bild](screens/bibliothek_tag_1440.png))
- Links 8 Spalten: Songliste, Zeilen 60 dp, 4 dp Abstand. Spalten: Name, BPM, Tonart-Chip, Länge. Die gewählte Zeile ist ink, klappt auf und zeigt die Abschnittsleiste (Intro, Teil, Break, Drop, Outro; der laufende Abschnitt grün) und "Auf Deck A", "Auf Deck B", "In die Mix-Liste" (primary), "Entfernen".
- Rechts 4 Spalten: Kachel "Songs hinzufügen" (accent, `head`-Titel, Button "Dateien wählen", Hinweis zum Hineinziehen), Kachel Bibliothek (große Zahl, Spielzeit, Analysestand), Kachel "Passende Tonarten" (2×12 Chips; passende hervorgehoben; Vorschlag).
- Leer-Zustand: die Songliste wird eine große Kachel mit dem Satz "Noch keine Songs" und der accent-Aktion "Songs hinzufügen". Importzeilen zeigen den Status (Dekodiere …, Analysiere Tempo, Tonart und Aufbau …, Import fehlgeschlagen) als `Status`.

### 7.2 Auto-Mix ([Bild](screens/auto-mix_tag_1440.png), [Nacht](screens/auto-mix_nacht_1440.png))
- Links 7 Spalten: Mix-Liste. Songzeile (nummernlos; der laufende Song ist ink mit einem grünen Quadrat links) und dazwischen eine **Übergangszeile** (44 dp, eingerückt 28 dp, tile2): Qualitätsquadrat (grün = Tonarten passen, gelb = weich), Typ in Medium, Beats und BPM-Wechsel, rechts Erklärung in small. Läuft der Übergang, füllt er sich. Darunter "Alle Songs hinzufügen" (primary) und "Leeren". Darunter das Display "Zeitleiste": Songs als überlappende Blöcke in Deck-Licht, Playhead.
- Rechts 5 Spalten: Kachel accent "Läuft" (Songname `head`, Restzeit `display` 52, Fortschrittsbalken weiß auf Weißton, "Stopp", "Nächster Song", Übergang in …), Kachel Übergänge (Segment Typ, Segment Länge, Segment Reihenfolge), Kachel Export (Titel, Satz, "Exportieren" primary).

### 7.3 Decks ([Bild](screens/decks_tag_1440.png), [Nacht](screens/decks_nacht_1440.png))
- Zwei gleich breite Decks (je 6 Spalten), darunter der Crossfader über die ganze Breite.
- Deck von oben: Kopf-Kachel (`pulse`): Buchstabe als 56-dp-Feldkachel (A schokolade, B schiefer), Songname `lead`, Zeit small, Tonart-Chip, BPM `display`. Display: Wellenform mit Takt-Bändern (Vierer-Gruppen abwechselnd screen/screen2), Playhead weiß, gespielter Teil hell, Rest 38 %; darunter 6-dp-Abschnittsleiste. Steuerkachel: Zeile "Pause" (accent, 52 dp, breit), Cue, Sync, Key; Tempo als bipolarer Balken; EQ als drei Kappen-Fader mit Kill darunter; in der Mitte Filter als 96-dp-Knopf (LP links, HP rechts); rechts Level mit Pegel.
- Crossfader-Kachel: A, Kappen-Fader, B; darunter Segment Kurve (Weich, Gleiche Lautstärke, Schnitt).
- **Telefon:** oben Segment "Deck A | Deck B" (zeigt je ein Deck), Crossfader bleibt unten.

### 7.4 Effekte ([Bild](screens/effekte_tag_1440.png))
- Oberes Drittel, drei Kacheln: Echo (6 Spalten: Titel, Satz, "An/Aus" rechts, Segment Abstand, Knöpfe Wiederholung und Pegel 72 dp), Hall (3, ein 88-dp-Knopf), Flanger (3, ein 88-dp-Knopf).
- Unten zwei Drittel: Beat Roll und Stutter je 4 Spalten mit Segment (Länge/Rate) und einer **großen Halten-Fläche**, die den Rest der Höhe füllt (Beat Roll Sand, Stutter Schiefer, Text unten links 28 sp); Granular-Freeze (4 Spalten) mit Segment 2/4/8 s, Display mit der letzten Aufnahme und primary "Einfangen und in Granular öffnen".
- Der Tipp-Text unter den Karten entfällt; "Halten" steht auf den Flächen.

### 7.5 Pads ([Bild](screens/pads_tag_1440.png), [Nacht](screens/pads_nacht_1440.png))
- Obere Leiste (volle Breite): Segment Muster (House, Techno, Hip-Hop, Breakbeat, Leer), kleine Knöpfe (40 dp) Lautstärke und Swing mit Wert, "Läuft" (primary) rechts.
- Links 5 Spalten: 4×4 Pads, füllen die Resthöhe (Gruppenfarben §3.1; Taste oben rechts). Rechts 7 Spalten: Sequencer als Display mit 16 Zeilen × 16 Schritten, Schritte 3-dp-Radius, Beat-Gruppen durch Takt-Bänder (screen/screen3), aktive Schritte in Gruppen-Licht, Playhead-Spalte heller, Kopfzeile mit 1 bis 4.

### 7.6 Granular ([Bild](screens/granular_tag_1440.png))
Das Instrument ist inhaltlich unverändert (hold/step/keys, 8/16, 16 Zellen, Hüllkurve, vier Stimmen, pad/drums/noise/sine, sieben Knöpfe), bekommt aber dasselbe Material: Display links (7 Spalten), rechts Steuerkachel und Hüllkurven-Kachel, unten eine Kachel mit den Knöpfen, `grains` als großer Knopf (92 dp) in der Mitte. Die Titel bleiben klein geschrieben wie im Original-Vorbild. Das beige "Palette.body" entfällt: ground/tile aus §3.1.

### 7.7 Telefon (390 dp, [alle Tabs](screens/telefon_alle_tabs_390.png))
Eine Spalte, Kacheln 8/6 dp Abstand. Oben Leiste: Wortmarke, BPM, Play. Unten die Navigations-Kachel. Songzeilen verlieren die Länge; Übergangszeilen verlieren den Erklärungssatz; Decks per Segment umschalten; Sequencer ohne Zeilenbeschriftung (Pad-Raster darüber trägt die Namen); Granular-Zellen 4 × 4.

## 8. Umsetzung (Vorschlag in Reihenfolge)

1. `design/AutoDjTokens.kt` einführen und Hanken Grotesk einbinden (Desktop: Resources, Android: `res/font/`). `DjColors`, `Palette`, `VoiceColors`, `DjTheme`, `AutoDjTheme` darauf zusammenführen; Material3-Komponenten (Chip, NavigationBar, Slider) nicht mehr direkt verwenden.
2. Bausteine: `Tile`, `Btn`, `Segment`, `Knob`, `BarFader`, `CapFader`, `Chip`, `Pad`, `Display`. Danach Tab-Leiste (Desktop) und Bottom-Leiste (Android).
3. Tabs nacheinander in dieser Reihenfolge: Decks, Pads, Effekte, Mix, Bibliothek, Granular. Jeweils Desktop und Telefon.
4. Puls (§5) und Einblend-Animation (§6) zuletzt, als eigene Schicht.
5. Nacht-Variante: nur anderer `AutoDjColors`-Satz, kein weiterer Code.

Hinweise: `fontFeatureSettings = "tnum"` auf allen Zahlen; keine Material-Ripple-Wellen (Druck = Skala + Tonwechsel); Fokusring der Desktop-Version als 2-dp-Fläche in ink2 (kein Rahmen-Look); Kontraste in §3.1 wurden gerechnet (nur Text auf tile2 mit ink2 liegt knapp bei 4.9:1).

## 9. Offen und Annahmen

- Beispielinhalte im Prototyp (Songnamen "Night Drive" usw., Zeiten) sind erfunden; sie ersetzen nur die Demo-Songs.
- "Passende Tonarten" ist ein Vorschlag, nicht im Code. Die Datenbasis (Camelot-Schlüssel je Song) ist da.
- Tasten der Pads (1 2 3 4 / Q W E R / …) sind ein Vorschlag; der Code kennt sie nicht.
- Die Nacht-Variante ist abgeleitet und noch nicht auf allen Tabs von Hand geprüft (Decks, Pads, Mix liegen als Bild vor).
- Nicht auf echter Hardware gesehen: Maße sind Designwerte; Plan: im CI-Screenshot gegen [screens/](screens/) vergleichen.
