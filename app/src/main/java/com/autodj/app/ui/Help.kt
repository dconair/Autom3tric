package com.autodj.app.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.autodj.app.design.AutoDjSpace
import com.autodj.app.design.AutoDjType
import com.autodj.app.design.Txt

/** The one line of explanation under the instrument: it shows what the control you touched last (or, with a mouse, hover over) does. */
class HelpState(initial: String) {
    var text by mutableStateOf(initial)
}

val LocalHelp = compositionLocalOf<HelpState?> { null }

/**
 * Makes [text] the help line while a finger or the mouse is on this element. It only watches the pointer (initial pass, nothing is
 * consumed), so the element keeps working exactly as before.
 */
fun Modifier.helpText(text: String): Modifier = composed {
    val help = LocalHelp.current
    if (help == null) {
        this
    } else {
        this.pointerInput(text, help) {
            awaitPointerEventScope {
                while (true) {
                    val e = awaitPointerEvent(PointerEventPass.Initial)
                    if (e.type == PointerEventType.Press || e.type == PointerEventType.Enter) help.text = text
                }
            }
        }
    }
}

@Composable
fun HelpStrip(help: HelpState, modifier: Modifier = Modifier) {
    Box(modifier.fillMaxWidth().padding(horizontal = AutoDjSpace.margin).padding(bottom = AutoDjSpace.margin, top = 4.dp)) {
        Txt(help.text, style = AutoDjType.small, second = true, modifier = Modifier.fillMaxWidth().padding(horizontal = 4.dp))
    }
}

/** The explanations of the instrument, in plain words. One place, so that they are easy to find and to translate. */
object InstrumentHelp {
    const val Start = "Tippe auf die dunkle Fläche, um zu spielen: Sie zeigt deine Datei als Welle. Links ist der Anfang, rechts das Ende. Ziehe, um die Stelle zu wechseln. Dreh- und Schaltflächen erkläre ich dir hier unten, sobald du sie berührst."
    const val Screen = "Die Welle ist deine Datei. Drücke und ziehe darauf: Von dieser Stelle werden winzige Schnipsel („Körner“) abgespielt. Loslassen beendet den Ton, außer „Abspielen“ ist an."
    const val Play = "Abspielen: Der Klang läuft weiter, auch wenn du nichts berührst. Nochmal tippen stoppt ihn."
    const val Load = "Eigene Datei laden: Wähle einen Song oder ein Geräusch (MP3, WAV, FLAC, AAC, OGG). Er erscheint danach als Quelle zum Spielen und liegt auch in der Bibliothek."
    const val Hold = "hold: Du hältst eine Stelle der Datei fest. Position (oder die Welle) bestimmt, welche. Gut für Flächen und Klangteppiche."
    const val Step = "step: Die Datei wird in 8 oder 16 gleich lange Stücke geschnitten, die im Takt nacheinander laufen. Unten baust du dein eigenes Muster aus den Stücken."
    const val Keys = "keys: Jede Taste spielt ein Stück der Datei, wie bei einem Sampler. Tippe die Tasten rhythmisch."
    const val Cells = "8 oder 16: In wie viele gleich lange Stücke die Datei für step und keys geschnitten wird."
    const val Envelope = "Hüllkurve: Die Form jedes Korns. Weiche Formen (hann, gauss) klingen sauber und fließend, harte (square) klicken und knistern."
    const val Voice = "1 bis 4: Das Plugin hat vier Stimmen. Jede kann eine andere Datei und andere Einstellungen haben, alle spielen zusammen."
    const val Source = "Quelle: Welche Datei die gewählte Stimme spielt. pad, drums, pluck und bass sind eingebaut, darunter stehen deine geladenen Dateien."
    const val Sync = "sync: Die Körner kommen genau im Takt (Raster unter „grains“). free: Sie kommen nach Zufallsprinzip mit der gewählten Dichte."
    const val Grains = "grains: Wie viele Körner pro Sekunde (free) oder in welchem Raster (sync). Wenige: einzelne Schnipsel. Viele: dichter, weicher Klang."
    const val Length = "length: Wie lang jedes Korn ist. Kurz klingt körnig und hell, lang weich und fließend."
    const val Position = "position: Von welcher Stelle der Datei die Körner kommen. Bei step und keys: Versatz innerhalb des Stücks."
    const val Tune = "tune: Tonhöhe in Halbtönen. +12 ist eine Oktave höher, -12 eine tiefer."
    const val Space = "space: Wie breit die Körner im Stereobild verteilt werden. 0 = Mitte, 100 % = überall."
    const val Chaos = "chaos: Mischt Zufall und Verzerrung dazu. Bei 0 klingt es sauber, mehr macht es wild."
    const val Output = "output: Gesamtlautstärke des Plugins in dB. 0 ist normal."
    const val Knob = "Drehregler: Maus gedrückt halten und nach oben oder rechts ziehen = mehr. Das Mausrad geht auch. Doppelklick setzt den Standardwert."
    const val BeatGrid = "Raster: Jede Spalte ist ein Schritt, jede Zeile ein Stück der Datei. Tippe einen Punkt, damit dieser Schritt dieses Stück spielt. Nochmal tippen macht daraus eine Pause."
    const val BeatPresets = "Vorlagen: Ein Fertig-Muster zum Ausprobieren. Danach kannst du jeden Punkt im Raster ändern."
    const val BeatStep = "Schrittlänge: Wie lang ein Schritt ist. 1/16 heißt: 16 Schritte sind ein Takt."
    const val BeatTempo = "Tempo des Rasters. Läuft ein Deck oder der Auto-Mix, richtet sich das Raster nach dessen Takt."
    const val BeatStarter = "Beat-Start: Stellt alles passend ein (Drums als Quelle, 16 Stücke, Raster im Takt, saubere Körner) und startet ein Muster."
}
