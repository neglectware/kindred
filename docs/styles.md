# Kindred style files

A style is one JSON file. It holds everything the engine composes from: modes, chord cells, bass plans, the color palette, weights and limits, the names and ranges of the character controls, and the defaults of a new project. The engine code has no genre in it.

- Built-in styles: `styles/*.json` in the source tree. They are compiled into the plug-in.
- User styles: `~/Library/Application Support/Neglectware/Kindred/Styles/*.json` (macOS) or `%APPDATA%\Neglectware\Kindred\Styles\*.json` (Windows). They are read when the plug-in loads. A file with an error is skipped, and the footer of the window shows the reason.
- `styles/warm-ambient.json` is the reference. It is written by `node tools/make-style.js` from the tables of the web prototype, so it is a transcription by program. `styles/open-road.json` is made by the same script as an example of a second style: 20 lines of changes to the first.

To make a style: copy `warm-ambient.json`, change `id` and `name`, change the data, and check it with `build-engine/KindredRules <file> 1800` (it rolls 1,800 projects and checks the structure and the rules).

## Top level

| Key | Meaning |
|---|---|
| `format` | `1`. |
| `id`, `name`, `description` | `id` is stored in projects. Do not change it after projects use it. |
| `defaults` | New project: `mode`, `tempo`, `bpb` (beats in a bar), `tie` (hold shared notes), `sections` (a list of roles), `arrangement` (pairs of [index into `sections`, bars]). |
| `modes` | The modes the style offers, in menu order. |
| `spectra` | The seven character controls. |
| `extended` | `name` and `hint` of the extended-colors switch. |
| `extensions` | The chord colors (the palette). |
| `colorWeights` | How likely each kind of color is. |
| `tokens` | The Roman numerals that cells may use. |
| `cells` | Four-chord cells, by mode and by kind. |
| `plans` | Bass plans. |
| `roles` | Section roles: names, allowed plans, plan weights. |
| `tension` | Tension of each chord degree (0 to 1). Suspensions land where tension falls. |
| `drone` | Which chords may sit over a drone. |
| `leadIns` | The short approach chords. |
| `edge` | Chords the Edge control may borrow. |
| `rules` | Numbers and switches (below). Every one is optional; the default is the Warm Ambient value. An unknown name is an error. |

## modes

`{ "id", "name", "steps": [7 semitone steps], "minor": bool, "borrow": ["iv", "bVII"], "planBias": { plan id or "*": factor } }`

- `minor`: the tonic chord is minor. "Major modes only" skips these modes, and the Brightness control does not apply in them.
- `borrow`: which borrowed chords the Surprise control may use. `iv` = the minor iv in place of IV before I. `bVII` = bVII in place of V or ii before I or IV. Empty = none.
- `planBias`: multiplies the weight of bass plans in this mode (Lydian leans on the drone).

## spectra

`{ "id", "name", "low", "high", "lowHint", "highHint", "min", "max", "default" }`

The seven ids are fixed, because each one is a rule in the engine: `wistful`, `moving`, `color`, `inner`, `surprise`, `leadins`, `edge`. A style sets their names, their end labels and their **range**: `min` and `max` (inside 0 to 1) are the ends of the slider. A style that must never be a pure drone sets `"min": 0.4` on `moving`.

## extensions and colorWeights

`extensions` has one list for each triad quality (`maj`, `min`, `dim`): `{ "s": suffix, "iv": [intervals], "t": tag }`.

The tag decides where the engine may use the color:

| Tag | Rule in the engine |
|---|---|
| `plain` | Always allowed. |
| `add9`, `maj7` | Allowed over a drone too (maj7 only on IV), if the added note is not a sixth above the bass. |
| `sus2`, `m7`, `madd9` | Not over a drone. |
| `sus4` | Only on the V chord, on its own root. |
| `jazz` | Only with the Extended colors switch, or with Edge above zero. Never over a drone. |

A color is used only if all its notes are in the parent scale of the chord.

`colorWeights[tag]` = `{ "base", "color", "plain", "degree", "degreeDefault", "degreeOwnRootOnly", "overDrone" }`. The weight is
`(base + color * C + plain * (1 - C)) * degree factor * drone factor`, where C is the Color control. `degree` maps `"7maj"` (degree and quality) or `"5"` (degree) to a factor. `overDrone` is the factor when the chord sits over a drone. `default` is used for tags without an entry.

## tokens and cells

`tokens`: `"IV": [5, "maj"]` = semitones above the tonic and the quality.

`cells[mode][kind]` is a list of `["I vi IV V", weight, drive]`. A cell has exactly four chords. `drive` is 0 for rocking in place and 1 for a full functional circle; the Bass motion control favors cells with more drive when it is high.

| Kind | Used for |
|---|---|
| `home` | Verse: starts on the tonic. Needed in every mode. |
| `lift` | Chorus and turnaround: starts off the tonic. Needed. |
| `away` | Middle eight with a moving bass. |
| `ped5` | Middle eight over the fifth. If a mode has none, plans on the fifth are not offered in it. |
| `close` | The last four chords of a coda. Each must end on the tonic. Needed. |

## plans

`{ "id", "label", "pattern" }`. A bass type is `"T"` (tonic drone), `"F"` (drone on the fifth) or `"R"` (the chord root).

- `{ "all": "T" }`: the whole section.
- `{ "half": ["T", "R"] }`: first half, second half.
- `{ "tail": ["T", "R"], "long": 2, "short": 1 }`: the first type, then the second for the last `long` chords (8-chord sections) or `short` chords (4-chord sections).

## roles

`{ "id", "name", "length", "plans", "planWeights" }`. The five ids are fixed (`home`, `lift`, `away`, `turn`, `return`): each is a rule for how the section is made. A style sets the display name (Verse or A section, Middle eight or Bridge), the default length, the allowed plans and their weights.

`planWeights[plan]` is a sum of terms `[c, d, m]` = `c * D^d * M^m`, where M is the Bass motion control and D = 1 - M. Example: `[[0.5, 0, 0], [2.4, 1, 1]]` = `0.5 + 2.4 * D * M`.

## drone, leadIns, edge

- `drone.tonicExcludes`: degrees that take their own root in place of the tonic drone (vi and iii: over the tonic they sound like added-sixth chords). `drone.fifthAllows`: degrees that may sit over the fifth.
- `leadIns`: `{ "w", "offset" (semitones from the target root), "q": "maj" or "target", "ext", "iv", "extMinor", "ivMinor", "sharp" }`.
- `edge.borrow`: `[degree, quality]` pairs. With Edge above zero, a roll may replace up to `rules.edgeMaxBorrowed` chords with one of these, if it shares a triad note with a neighbor and is not in the mode.

## rules

| Name | Default | Meaning |
|---|---|---|
| `voiceLo`, `voiceHi` | 50, 79 | Range of the upper voices (MIDI notes). |
| `voiceCenter`, `voiceMaxSpan` | 63, 19 | Where voicings sit, and their largest span. |
| `voiceLowLimit`, `voiceLowMinGap` | 57, 5 | Below this note, neighbors are at least this far apart. |
| `bassLo`, `bassHi` | 33, 45 | Bass range. |
| `maxDistinct` | 4 | Different chords in a section (plus borrowed). |
| `borrowChance`, `borrowChanceAway` | 0.55, 0.9 | Chance of a borrowed chord at Surprise = 1 (other sections, middle eight). |
| `cellDriveSlope`, `cellDrivePivot`, `cellMinWeight` | 1.6, 0.4, 0.15 | How Bass motion favors cells with drive. |
| `minorBase`, `minorSlope` | 0.15, 2.2 | Weight of each minor chord in a cell = base + slope * Brightness. |
| `pedalMisfit`, `lockedMatch`, `lockedMiss`, `homePlanRepeat` | 0.3, 8, 0.5, 0.35 | Cell and plan weight factors. |
| `connectBoost` | 1.2 | How much a color is favored when a neighbor has the same note. |
| `susCapBase`, `susCapSlope`, `susRepeatMax` | 0.15, 0.35, 2 | Most suspensions in a section, and the most uses of one suspension. |
| `susReleaseDrop`, `susReleaseMul`, `susReleaseMax`, `susOtherMul`, `susLastBase`, `susLastSlope` | 0.15, 1.2, 0.9, 0.2, 0.4, 0.6 | Chance of a suspension where tension falls, elsewhere, and on the last chord of a coda. |
| `tailChance` | 0.6 | Chance of a passing note at Inner movement = 1. |
| `chordVelocity`, `bassVelocity` | 72, 80 | Velocities in exported MIDI files. |
| `edgeColorWeight`, `edgeBorrowChance`, `edgeDisconnect`, `edgeMaxBorrowed` | 1.1, 0.8, 0.5, 2 | The Edge control: weight of extended colors, chance of an edge chord, chance that a change may share no note, most borrowed chords. |
| `seamKeepsNeighbor` | true | A fix to the prototype (see NOTES.md). `false` = exactly as the prototype. |

## What still needs code

A new **rule** needs a small change in `src/engine/Engine.cpp`: a new role behavior, a new color tag with its own restriction, a new kind of inner movement, or chord lengths that differ inside a section. Everything in the tables above is data.
