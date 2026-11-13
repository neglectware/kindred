# Kindred user manual

Kindred writes chord progressions and arranges them into a song. It sends MIDI. It makes no sound of its own in a DAW: put it before an instrument.

Version 0.9.1. Formats: Audio Unit MIDI effect (Logic Pro, MainStage), VST3, and a standalone application. macOS 10.13 or later (Intel and Apple silicon). Windows 10 or later, 64-bit (VST3 and the application; see `INSTALL-Windows.txt`).

## 1. Quick start in Logic Pro

1. Make a software instrument track and choose an instrument (a pad is a good first choice).
2. Click the **MIDI FX** slot of the channel strip.
3. Choose **Audio Units > Neglectware > Kindred**.
4. Press Play in Logic. Kindred plays its arrangement from bar 1, in time with Logic.
5. Click **Roll** to get new chords.
6. Click the lock on each chord that you want to keep.
7. Click **Roll** again. The locked chords stay. The other chords change.
8. When the progression is right, drag the **Drag arrangement** tile to a track. Logic makes a MIDI region.
9. Turn off Kindred on that track (or remove it), so that the region and Kindred do not play together.

## 2. Chords and bass on two tracks

A MIDI effect in Logic sends notes only to the instrument on its own track. To send the bass to a second instrument, use two instances of Kindred that share one progression.

1. Put Kindred on the chord track (section 1).
2. Make a second software instrument track with a bass sound.
3. Put Kindred in the MIDI FX slot of the bass track.
4. Look at the **Link** letter at the top right of each window. The two letters must be the same. A new instance uses **A**.
5. Look at the **Part** control. The second instance sets itself to **Bass**. The first instance sets itself to **Chords**. If you have set a part by hand before, Kindred does not change it: set **Chords** on one and **Bass** on the other.
6. Press Play. The two tracks play the same progression.

Each change that you make in one window appears in the other window. Each instance saves the full project with the Logic project.

To use two different progressions in one Logic project, give the second pair a different Link letter, or choose **Not linked**.

Note: all open projects in one host share the Link letters.

## 3. The window

- **Header.** Style, key, mode, meter, tempo, the play button, Part, Link and the menu.
- **Left column.** The Roll button, Undo and Redo, the character controls and three switches.
- **Section tabs.** One tab for each section (Verse, Chorus, Middle eight, Coda). The **+** adds a section.
- **Stage.** The chords of the section as cards. Below the cards, each note is a line at its pitch. The keyboard at the left shows the pitch. The bass has its own lane at the bottom.
- **Arrangement.** Blocks in play order. Each block plays one section for a number of bars.
- **Footer.** A help line for the control under the pointer, and two tiles to drag MIDI out.

### Menus

- Click a control with a small arrow to open its menu. Click a row to choose it.
- You can also press the mouse button on the control, move to a row, and release.
- To close a menu and change nothing: click anywhere outside it, click the control again, or press Escape. That click does nothing else.
- With the keyboard: Up and Down move, Return chooses, Right opens a submenu, Left closes it.
- A right-click on a section tab, a chord card or an arrangement block opens a menu for that item.
- To rename a section, double-click its tab and type. Return keeps the name. Escape drops it.

### How to read the stage

- A card shows the chord name, then its Roman numeral. `F/C` is an F chord over a C in the bass.
- Color shows the function of the chord: amber for the tonic, teal for subdominant chords (IV, ii), coral for dominant chords (V, bVII), violet for the relative minor chords (vi, iii), pink for a borrowed chord.
- A pale line that goes through two or more chords is a note that the chords share. It is held, not played again.
- A thin curve joins the end of one note to the note that the same voice plays next.
- `F -> E held` on a card: the F from the chord before is held, then it falls to E.
- `-> B passing`: one voice steps through B on its way to the next chord.
- In the bass lane, an amber bar with the word "drone" is a bass note held under changing chords.
- A locked chord has a bright frame and a closed lock.

## 4. Roll, lock, re-roll

- **Roll** (key R) writes all sections again. It writes the verse first, then makes the other sections from it.
- **Roll section** (Shift-R) writes only the section on the stage.
- The **dice** on a card changes only that chord. It chooses a chord that fits the chords before and after it.
- The **lock** on a card keeps the chord, its bass note and its held notes through every roll. The key L locks the card that you clicked last.
- The lock button next to Roll section locks or unlocks all chords of the section.
- **Undo** (Cmd-Z) and **Redo** (Shift-Cmd-Z) go back and forward through rolls and edits.
- Click a card to hear the chord when playback is stopped.

A rolled chord never changes by itself. The project stores each note, not a random seed.

## 5. Character controls

The controls change the next roll. They do not change chords that are there already.

| Control | Left | Right |
|---|---|---|
| Brightness | Bright: major chords | Wistful: more minor chords |
| Bass motion | Drone: the bass holds one note | Moving: the bass follows the chords |
| Color | Plain: triads | Rich: add9, maj7, sus |
| Inner movement | Still | Flowing: suspensions and passing notes |
| Surprise | In key | One borrowed chord in a section |
| Lead-ins | None | A short chord before a change, often |
| Edge | Pretty: each change shares a note | Strange: extended colors, distant chords, some changes with no shared note |

Double-click a control to set its default value.

Switches:

- **Hold shared notes.** On: a note that two chords share is one long note. Turn it off for a piano or for any sound that dies away.
- **Extended colors.** Lets the roll use 6, 6/9, maj9, m9, m11 and #11 chords.
- **Major modes only.** A roll does not use a mode that has a minor tonic chord.

## 6. Sections

Above the cards:

- **Role.** Verse, Chorus, Middle eight, Turnaround or Coda. The role sets how the section is made from the verse.
- **Chords.** 4 or 8.
- **Bars each.** 1, 2 or 4 bars for each chord.
- **Bass.** The bass plan: drone throughout; drone, then moving; moving, then drone; moving; held over the fifth. **Auto** lets the roll choose.
- **Loop section.** Plays only this section, again and again.

When you change one of the first four, Kindred rolls the section, unless the section has locked chords. Then you roll it yourself.

Double-click a tab to change the name of the section. Right-click a tab for more commands.

## 7. Arrangement

- Click a block to show its section.
- Drag a block to move it.
- Drag the right edge of a block to change its length. A block that is longer than its section plays the section again.
- Right-click a block to change its section or its length, to copy it, or to remove it.
- **Add block** adds a block of the section on the stage.
- **Repeat** starts the arrangement again when it ends.

## 8. Timing

- In a DAW, Kindred follows the tempo, the position, the loop and the meter of the host. The arrangement starts at bar 1.
- When the host stops, Kindred stops all notes. It sends a note-off for each note.
- When the host is stopped, the play button in Kindred starts its own clock at the tempo of the host. This is for a quick listen.
- The meter follows the host. To set it by hand, click the meter in the header. A meter change keeps all chords and changes only the bar lengths.

## 9. MIDI out to the timeline

- **Drag section** and **Drag arrangement** make a MIDI file and start a drag. Drop it on a track.
- The Part control sets what the file contains: chords and bass (the bass is a second track on channel 2), chords only, or bass only.
- The menu also has **Export ... as MIDI file**.

## 10. Performance settings

Menu > **Performance settings**. These belong to one instance, and the host can automate them.

- Chord velocity and bass velocity.
- Chord octave and bass octave.
- Humanize: small changes of velocity and timing.
- Strum: the notes of a chord start one after the other, from the lowest note.
- Note length.
- Chord MIDI channel (default 1) and bass MIDI channel (default 2).
- MIDI thru: notes that come into Kindred go on to the instrument.

## 11. Projects

- The DAW project saves everything: sections, locks, the arrangement and all controls.
- Menu > **Save project as** writes a `.kindred` file. **Open project** reads it.
- **Open project** also reads the `.json` project files of the earlier web versions (Chord Engine formats 1, 2 and 3). The chords, the notes and the locks stay the same.
- **Copy chord chart** puts the chords on the clipboard as text.

## 12. The standalone application

1. Open `Kindred.app` (macOS) or `Kindred.exe` (Windows).
2. Click **Options** in the title bar, then **Audio/MIDI Settings**.
3. Choose a **MIDI Output**. To play an instrument in a DAW on macOS, choose an IAC Driver bus (turn it on in Audio MIDI Setup first). On Windows, use a virtual MIDI port.
4. Press the play button, or the space bar.

The application has a soft pad sound of its own. Turn it off in Menu > Performance settings when you use a MIDI output. Chords go out on channel 1 and the bass on channel 2.

## 13. Styles

A style sets what kind of music Kindred writes. Choose it in the header. **Warm Ambient** is the main style. **Open Road (example)** shows a second style.

A style is a data file. To add one, put a `.json` file in the Styles folder (Menu > Open the Styles folder; on macOS it is `~/Library/Application Support/Neglectware/Kindred/Styles`, on Windows `%APPDATA%\Neglectware\Kindred\Styles`) and open the plug-in again. The file format is in `styles.md`.

## 14. If something is wrong

- **A menu stays open.** Click anywhere outside the menu, or press Escape. Then tell the developer which menu it was and where you clicked.
- **No sound.** Kindred sends MIDI only. Make sure that the track has an instrument and that the host is playing.
- **A note does not stop.** Menu > **Stop all notes**. Then tell the developer what you did before it happened.
- **Chords play two times.** A dragged region and Kindred are on the same track. Turn off Kindred.
- **The bass plays on the chord track.** Set Part to Chords on that instance.
- **Kindred is not in the MIDI FX list.** Start Logic again. Logic scans new Audio Units when it starts.

Credits: Kindred is AI-authored and human-directed. Fonts: Inter and Instrument Serif (SIL Open Font License).
