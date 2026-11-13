# Kindred: visual design

Written before the UI was built (2026-11-12), then updated with what the screenshots showed.

## 1. References, and what was taken from each

The reference images are vendor product screenshots, downloaded on 2026-11-12 and looked at for craft level only. They are not stored in this folder. Sizes below were measured on those images.

| Reference | Measured | Taken | Not taken |
|---|---|---|---|
| **Scaler 3** (composition view, 850 x 561 image) | Header 34 px of 561 (6%). Chord chip 51 x 32 px (6% of width), two lines: name at about 8 px bold, function numeral under it at about 6 px. One accent hue (blue) for all chips, grey for out-of-scale chords. | Two-line chip: chord name first, Roman numeral second. A flat, dark surface with one accent and thin separators. A single-row transport and tempo read-out in the header. | Five nested tab levels and a 62 px icon rail. The plug-in needs several clicks before a first sound. |
| **Scaler 3.3** (arrange view, 1654 x 1116 image) | Chord blocks 80 x 62 px on a bar ruler. Under them, one lane per voice, each note a 3 px line at its pitch height; lane height 70 px. | The best idea found: **voices drawn as lines under the chord blocks**. Kindred makes this the center of the window, in one shared lane, so a held note is visibly one line through several chords. | The mixer and the per-voice instrument slots. |
| **Captain Chords 3** (2142 x 1328 image) | Left column 420 px of 2142 (20%) for key, scale and character controls. Section tabs (Verse, Pre chorus, Chorus, Drop) across the top, each 288 px (13%). Chord blocks overlaid on a piano roll, numeral at 2x the size of the name. | Section tabs across the top. A left column of about 20% for "what kind of music" controls. Chords tied to a pitch display. | The piano roll as the main editing surface (too dense: 40+ note labels on screen). |
| **Xfer Cthulhu** (1193 x 1213 image) | A keyboard strip 100 px high along the bottom, sounding keys lit in one saturated color. Lock icons on each of 8 note slots. | Sounding notes lit on a keyboard. A lock on every slot, always in the same corner. | Bevels, skeuomorphic chrome, 12 px all-caps condensed labels. |

Common to all four: a dark theme, chord names as the largest text in the content area, and fewer than 3 accent colors.

## 2. Decisions

- **One window, no modes.** Everything in the 10-second flow (style, key, mode, Roll, lock, drag) is visible at once. No tabs except the section tabs.
- **The music is the picture.** The center of the window is a "voice stage": chord cards on top, and under them one continuous lane (D3 to G5) where each sounding note is a rounded line. A note that two chords share is one unbroken line. A suspension is a short step down. A passing note is a short step toward the next chord. The bass has its own lane below (A1 to A2), where a drone is one long bar.
- **Color means harmonic function**, not decoration: tonic amber, subdominant teal, dominant coral, relative minor chords violet, borrowed chords pink. The same color is used for the card edge, the Roman numeral and the notes of that chord in the lane.
- **Locked versus free at a glance:** a locked card has a filled lock, a bright frame and a solid surface. A free card has no frame. The dice (re-roll) is only on free cards.
- **Type:** Inter for all UI text (tabular figures for bar numbers), Instrument Serif Italic for the wordmark only. Chord names 25 px semibold at the base size.
- **Geometry:** base size 1120 x 720 (14:9). The editor scales as a whole, so every size is the same picture: 840 x 540 minimum, 2240 x 1440 maximum. Header 56 px, left column 232 px (21%), section tabs 40 px, voice stage 380 px, arrangement 150 px, footer 54 px.
- **Motion:** a 60 Hz timer drives the playhead, a glow on sounding notes and a 180 ms ease on card changes after a roll. No animation runs when nothing changes (the timer only repaints the moving parts).
- **Themes:** dark by default, light available from the menu. Both are one table of colors (`src/ui/Theme.h`).

## 3. Result

See section 4 after the build (measurements of the shipped screenshots).

## 4. Result (measured on the shipped screenshots, 2026-11-12)

Screenshots: `docs/screenshots/` (PNG at 2x). They are rendered offscreen by `tools/Snapshot.cpp` from the real editor.

| File | What it shows |
|---|---|
| `01-default-dark.png` | A fresh roll, stopped. 2240 x 1440 px (window 1120 x 720). |
| `02-playing-locked-dark.png` | Three locked chords, host playing in bar 3. |
| `03-hover-card-dark.png` | Pointer over a card: lock and dice appear, help line in the footer. |
| `04-middle-eight-dark.png` | Playback in the middle eight; the stage follows the song. |
| `05-playing-light.png`, `06-stopped-light.png` | Light theme. |
| `07-small-840-dark.png` | Smallest window, 840 x 540. |
| `08-large-1680-dark.png` | 1680 x 1080 at 1x. |
| `09-lydian-four-chords-dark.png` | F Lydian over a drone, four chords of two bars. |

Geometry at the base size: header 56 px (7.8% of the height; Scaler 3: 6%), left column 232 px (20.7%; Captain Chords: 20%), chord card 96.8 x 98 px with the name at 25 px and the numeral at 11.5 px, voice lane 822 x 170 px (4.7 px per semitone, note lines 4.6 px), bass lane 58 px, arrangement blocks 82 px high. Accent colors on screen at once: 5 function colors and no others.

Paint time (software renderer, offscreen, including the image allocation; `tests/out/snapshot.log`, last run):

| Window | Full repaint, mean | A playing frame (stage + arrangement), mean / worst |
|---|---|---|
| 840 x 540 @2x | 5.4 ms | 4.4 / 6.7 ms |
| 1120 x 720 @2x | 8.2 ms | 9.8 / 18.2 ms (6.7 / 7.3 ms on a quiet machine) |
| 1680 x 1080 @2x | 17.6 ms | 11.8 / 23.3 ms |
| 2240 x 1440 @2x | 37.4 ms | 24.9 / 37.6 ms |

One 60 Hz frame is 16.7 ms. So the playhead holds 60 Hz at the default size and drops below it at the two largest sizes on a 2x display. Before text shaping was cached, a full repaint took 60 ms. The render thread never waits for the editor: it reads one finished, immutable timeline and only takes a try-lock to pick up a new one.

What changed after looking at the first screenshots: cards kept a color wash from the change animation (now 420 ms and gone); voices were separate bars with no relation (added the thin curves from each note to the next note of the same voice); the window was flat (two soft glows, baked into a cached background image); the section settings did not fit beside the tabs (moved onto the stage); the arrangement panel had a 24 px empty band (blocks made taller).

Not done: the editor has no accessibility tree (it is one custom-drawn component).

## 5. Menus (2026-11-13)

The menus are part of the picture. The editor draws them in its own window, with the same type, colors and scale as the rest: a rounded panel under the control (above it when there is no room below, always inside the window), rows of 25 px at the base size, an amber check for the current value, an amber wash on the row under the pointer, and a submenu to the right of its row. The performance panel and the rename field are child components of the editor. Nothing opens a second window.

The reason is behavior, not looks: see `NOTES.md`, "The menu fault". A menu in its own window depends on the host's window focus, and in a host that is not ours to control.

| File | What it shows |
|---|---|
| `10-menu-key-dark.png` | The Key menu, pointer on G, check on the current key. |
| `11-menu-block-submenu-dark.png` | Right-click on an arrangement block, with the Bars submenu. |
| `12-menu-main-dark.png` | The main menu. |
| `13-performance-panel-dark.png` | The performance panel. |
| `14-menu-mode-light.png` | A menu in the light theme. |
| `15-performance-panel-small-840-dark.png` | The performance panel in the smallest window. |

A full repaint with a menu open: 8.1 ms mean at 1120 x 720 @2x (the same as without one).
