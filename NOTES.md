# Kindred: notes

Built 2026-11-12, revised 2026-11-13 (build 2: the menu fault, macOS 10.13 universal, Windows). A person has used build 1 in Logic Pro as an AU MIDI effect ("looks and works great except for one thing": the menus, see "The menu fault"). Build 2 has not been opened in a host by a person. Nobody has run the Windows build. What programs checked is under "Test results"; what nobody checked is under "Not verified".

## What it is

Kindred is a web prototype (called v2 below; its engine is in `tests/golden/prototype/`) as a native plug-in: AU MIDI effect (`aumi Kndr Nglw`) for Logic, VST3, and a Standalone app. JUCE 8.0.8, CMake. macOS: universal (x86_64 and arm64), 10.13 or later. Windows x64: VST3 and Standalone, built by `.github/workflows/build.yml`. Version 0.9.1 build 3. About 6,000 lines of C++, JavaScript and shell in `src/`, `tools/` and `tests/`.

## The name

Working name: **Kindred** (chords that are related; v0's fault was "the chords are not related enough"). To rename: change the eight lines of `src/Product.h` and rebuild. CMake reads the product name, target name, company, codes and bundle id from that file. The user data folder (`~/Library/Application Support/Neglectware/Kindred`) follows the name.

Candidates and checks (web searches on 2026-11-12, 13 queries; **these are searches, not a trademark clearance**):

| Candidate | Result in music technology | Verdict |
|---|---|---|
| **Kindred** | No plug-in, app or sample product found in 3 searches. Nearest: "KINDZAudio KinLab" and "Kindza" (different words). Outside music: a health-care company, a novel, a TV series. | Chosen |
| Plagal | Nothing found (2 searches). Only a MuseScore user name. | Clear, but it names one cadence and is jargon |
| Common Tone | Nothing found. | Clear, descriptive, plain |
| Tideline | Nothing found. | Clear, but says "ambient" only |
| Evenfall | "Evenfall" is a lo-fi sample pack by Loop Cult. | Rejected: collision |
| Pedalpoint | No plug-in of that name; several "Pedal..." products. | Clear, but names one technique |
| Throughline | Nothing found in music technology; a well-known podcast has the name. | Weak |
| Lodestar | Nothing found. | Clear, generic |

Manufacturer code: `Nglw` (the Neglectware code Grand Old Piano already uses; AX330G uses `Mobr`, so there is no clash). Plug-in code: `Kndr`. Bundle id: `com.neglectware.kindred`.

## Architecture

```
src/Product.h              the one place that names the product
src/engine/                pure C++17, no JUCE (builds and tests headless)
  Json.*                   small JSON value, parser, writer
  Style.*                  the style file loader (docs/styles.md)
  Engine.cpp               roll: skeleton, bass plan, color, voicing, inner movement
  EngineIO.cpp             events, ties, MIDI file, project storage and migration, chart
src/plugin/
  Session.*                the shared project: snapshots, undo, link groups, style library (no JUCE)
  Player.h                 timeline -> MIDI, sample-accurate, no allocation (no JUCE)
  PluginProcessor.*        JUCE processor: host sync, parameters, state, drag files
  PadSynth.h               the soft pad of the Standalone app
src/ui/PluginEditor.*      the editor (one custom-drawn component)
styles/*.json              built-in styles
tools/                     make-style.js, Snapshot, HostTest, UiTest, build.sh, test.sh, minos.sh
tests/golden/prototype/    the JavaScript engines of the prototype (the reference of the golden test)
.github/workflows/build.yml  Windows x64 zip, macOS universal bundles, tests (no publishing)
tests/                     golden/ (fixtures from the prototype), RulesTest, PlayerTest
```

### Decisions and reasons

- **The engine is a line-by-line port.** Same random generator (mulberry32), same order of random draws, same order of floating-point operations. The engine library is compiled with `-ffp-contract=off`, because a fused multiply-add changes a weight by one unit in the last place and can pick a different chord.
- **Styles are data.** Every table and number of v2 moved into `styles/warm-ambient.json`, written by a script from v2's own tables. A second style (`open-road.json`) is 20 lines of changes and needed no engine code.
- **Chords and bass on two tracks: link groups.** Instances with the same Link letter (A to H) in one host process hold one shared `Session`. Each has a Part parameter (Chords + Bass, Chords, Bass). A new instance joins link A. When a second instance joins a group, it sets itself to Bass and the first to Chords, unless someone has set those parts by hand. Each instance saves the whole project and a stamp (lineage id, edit count, time) in its own state; on load the newest copy wins, so the result does not depend on the order the host restores instances. Why this way: a Logic MIDI effect can only feed its own track, AUv2 instances share a process, and a registry in a static is the simplest thing that is reliable there.
- **Host sync.** Each block, the processor maps the host's PPQ position to a position in the timeline and asks `Player` for the MIDI of that block. `Player` keeps a count for every (channel, pitch) it has turned on. Stop, bypass, `reset()` and `prepareToPlay()` end each sounding note with its own note-off, then send CC 64 = 0 and CC 123 as a backstop. A locate, a loop or a new roll in mid-chord is settled by a difference: notes that should no longer sound get a note-off, notes that should sound get a note-on, notes common to both are left alone (so a held pad is not struck again). Small position differences between blocks (tempo ramps) are treated as clock noise and never send a note twice.
- **Threads.** Every edit makes a new immutable project snapshot under a mutex. The processor builds an immutable `RenderData` on the editing thread and hands it to the render thread through a try-lock swap. The render thread never allocates and never blocks. The editor polls the revision number at 60 Hz.
- **Meter and tempo follow the host.** A meter change keeps every chord and refits bar lengths (`Engine::refitMeter`). Meters whose bar is not a whole number of quarter notes (7/8) are not followed.
- **State.** One JSON document: project, play settings, parameters, link, window size. Project format 4 (v2 was 3): `officeSafe` became `majorModesOnly`, `jazz` became `extended`, plus `style` and `mood.edge`. Formats 1 to 3 open through the same migration v2 had.
- **Language.** The prototype's switch for major-sounding modes (file key `officeSafe`) is now "Major modes only" (off by default in a new project; a v2 file keeps its setting). "Jazz colors" is "Extended colors". Like / Not this was a tuning aid for one person and is not in the UI; the `feedback` list of an imported file is kept in the project.

## Changes to v2's behavior (all listed)

1. **Fix behind style data, ON in the shipped style: `rules.seamKeepsNeighbor`.** v2 states "every chord change keeps at least one pitch class" as a hard rule. Its loop-seam pass could break it: when the last chord was recolored to share a note with the first, it was allowed to lose its link to the chord before it (example found: A-flat to B-flat-m6 sharing nothing). With the switch off the engine is exactly v2. With it on, 73 of 3,102 golden fixtures differ.
2. **New: the Edge control (Pretty to Strange).** At 0 (the default) it does nothing and draws no random numbers; the golden test proves that. Above 0 it allows extended colors, lets a roll borrow up to 2 chords from the style's `edge.borrow` list (each must share a triad note with a neighbor), and lets some changes share no note. Measured at random Edge settings: 145 of 15,028 changes shared no note (1.0%).
3. **New: meter refit**, and a bass-only MIDI file.
4. Two v2 test assertions are statistical in fact and are now bounded counts, not assertions: a top-voice leap over a sixth (seen 1 time in 45,044 chord changes: a tenth, in a Dorian middle eight) and a section with 6 different chords (seen 1 time in 7,753 sections: a coda whose verse had four chords and no IV). Both are v2 behavior; v2's own test did not hit them at 7 seeds. They are not fixed.

## Test results (2026-11-13, build 2)

Run them: `tools/test.sh` (engine) and `tools/build.sh` (plug-in). Logs: `tests/out/`.

| Test | Result |
|---|---|
| **Golden equivalence** with `engine.js` (`tests/golden/`) | Run as arm64 and as x86_64 (Rosetta): the same result. 3,102 fixtures (2,500 random projects, 500 lock runs of about 60 operations, 102 migration files: 2 hand-built and 100 from the real v1 engine). 55,670 operations, 61,750 rolls, **80,894 comparisons, 0 mismatches**: project state after every operation, final project text, raw and tied event lists, arrangement and section MIDI bytes, chord chart text, and exact save and reload. Migration MIDI bytes also equal the v1 engine's own output. |
| Is the golden test sensitive? | One style constant changed from 1.2 to 1.25: 1,187 mismatches in 378 fixtures. |
| **Rule checks** (`tests/RulesTest.cpp`, every check of v2's `test.js`) | Warm Ambient: 1,800 projects, 1,000,855 checks, 0 failed (seed 20261111). Also 0 failed at 25 other seeds. Open Road: 1,800 projects, 1,218,370 checks, 0 failed. Includes 300 lock runs of 50 rerolls (2,865 locked slots compared), migration, 60 meter refits. |
| **Scheduler stress** (`tests/PlayerTest.cpp`) | 2,000 scenarios, 757,284 blocks, 24,045 stops, 23,912 locates, 4,395 loop wraps, 24,111 timeline swaps in mid-chord. 169,740 note-ons, 169,740 note-offs. Stuck notes: 0. Note-on for a sounding pitch: 0. In 400 straight runs, all 32,015 notes started within 1.5 samples of their time. |
| **Host test** (`tools/HostTest.cpp`, the built VST3 loaded through JUCE hosting) | Run as arm64 and as x86_64: the same result. 40 checks, 0 failed, 60,047 blocks: start, stop, 40 locates, 20 loop passes with tempo changes, 400-step tempo ramp, meter change while playing, 4 sample-rate and block-size pairs (44.1 kHz/64 to 192 kHz/4096), 411 note-ons = 411 note-offs, state restore plays the same 41 notes, damaged state ignored, two linked instances (automatic Chords and Bass; their notes together equal one instance playing both), bypass, `reset()`. |
| **UI click-through and menus** (`tools/UiTest.cpp`, synthetic mouse and key events) | **625 steps, 0 failed**, the same as arm64 and as x86_64. 33 control steps at 3 window sizes (99). Menus: 15 controls that open a menu x 3 sizes = 45 menus, each 10 checks: opens on one click inside the editor with no new window and nothing modal (45), still open after 300 ms with no input (45), second click on the control closes it and it stays closed (45), a click elsewhere in the editor closes it and is not passed on (45), a press outside the editor closes it (45, at the seam: the test calls what the timer calls), Escape (45), resize (45), hiding the editor (45), press and release on a row chooses it (45), nothing left behind (45). Also: press-drag-release, submenu open and close and choose, keyboard, a second control while a menu is open, the performance panel (7 checks per size), rename in place (3 per size), the editor closed with a menu or the panel open (0 windows, 0 modal components left), the rule that tells an outside press from one of our own (12 cases), the system press clock. |
| The old popup, measured (`KindredUiTest --legacy`) | `juce::PopupMenu` shown as before build 2: top-level windows 0 to 1, modal components 0 to 1; the window is temporary and cannot become key; with no input it closed itself after 71 ms (the test process is not the foreground application); `inputAttemptWhenModal()` closed it at once. |
| **auval** `auval -v aumi Kndr Nglw` | AU VALIDATION SUCCEEDED for the arm64 half (`tests/out/auval.log`). It must run from Terminal.app: a background shell sees no third-party Audio Units. `tools/build.sh` opens `tools/auval-in-terminal.command` for that. The Intel half: see "Portability". |
| **pluginval** | **Not run.** The session was not allowed to fetch and build external source. To run it: get pluginval from Tracktion, then `pluginval --strictness-level 10 --validate ~/Library/Audio/Plug-Ins/VST3/Kindred.vst3`. |
| Standalone | Launched for 7 s as arm64 (2.1% CPU, 115 MB) and as x86_64 under Rosetta (2.1% CPU, 80 MB): no crash. Its window, its sound and its MIDI output were not checked. |

## Not verified

- **Build 2 in Logic Pro.** The menus were changed because of what a person saw in Logic, and no person has seen the new menus there. See "What only a person in Logic can confirm" below.
- **Sound by ear** beyond the first session in Logic: the Edge control and the Open Road style pass the structural checks; whether they sound right is unknown.
- **Logic Pro details.** Unknown: that Logic calls the plug-in while stopped (needed for the preview button and for hearing a clicked chord), how Logic treats the dragged file (it may ask about the tempo in the file), that two linked instances always land in the same process (Logic ran Kindred in its Audio Unit helper process together with another vendor's plug-in, so one helper holds several plug-ins; two Kindred instances were not looked at), and that Logic follows the meter and loop the way the host test simulates them.
- **Drag to the timeline.** The file is written and checked (type 1, 3 tracks). The drag itself (`performExternalDragDropOfFiles`) was never done with a real mouse by the tests.
- **VST3 in a real host.** Only JUCE's own hosting. Some hosts do not route MIDI out of a VST3 effect. The VST3 wrapper adds 2,081 MIDI CC parameters next to Kindred's 11 (JUCE default).
- **Windows.** Everything: that it compiles with MSVC, that the tests pass there, that the plug-in loads. See "Portability".
- **A real Intel Mac and a real macOS 10.13.** The Intel half was run only under Rosetta on macOS 15. See "Portability".
- **pluginval** (above), the file dialogs, the light theme outside screenshots, user style folders with real files, MainStage.

## Known gaps

- All v2 weaknesses remain: one chord length per section, close four-note voicings only, ties stop at block edges, the Dorian and Minor cell lists got little attention.
- Link letters are shared by every open project in one host process.
- The arrangement always starts at host bar 1 (`startBar` is in the state but has no control yet).
- No accessibility support in the editor.
- At the two largest window sizes on a 2x display, a playing frame takes longer than one 60 Hz frame in the offscreen measurement (11.8 and 24.9 ms mean).
- 7/8 and other meters without a whole number of quarter notes are not followed.
- Signing is ad hoc (local use). No installer, no notarization. On another Mac, Gatekeeper will stop the unsigned bundles until they are signed and notarized, or the quarantine flag is removed.
- The Windows build is not signed and has no installer.
- The performance panel uses JUCE's stock sliders and font, not the editor's own drawing.
- A menu has no type-to-select and no scrolling: a list taller than the window is laid out in columns.

## The menu fault (2026-11-13)

**Report** (Logic Pro, AU MIDI effect): a menu sometimes stayed after a click away from it; later a menu flashed for a fraction of a second and closed; later it worked. Intermittent.

**Where the plug-in runs.** Seen on the machine of the report while Kindred was open in Logic Pro 12.3 (macOS 15.7, Apple silicon; `lsof`, `lsappinfo`): Kindred's code is not loaded in the Logic Pro process. It is loaded in `AUHostingServiceXPC_arrow`, shown by the system as "AUHostingService (Logic Pro)": a helper process of type BackgroundOnly, sandboxed. Logic shows the plug-in's view in its own window, but the plug-in, JUCE and every window JUCE makes belong to the helper. Mouse presses in Logic's windows go to Logic and never reach the helper.

**Cause.** Up to build 1 every menu was a `juce::PopupMenu` shown with `withParentComponent(nullptr)` and no target component: a separate top-level window (JUCE: `MenuWindow`, added to the desktop with `windowIsTemporary | windowIgnoresKeyPresses`, and modal). JUCE 8.0.8 closes such a window in four ways only:

1. A mouse press on one of JUCE's own windows that the menu blocks (`Component::internalMouseDown` -> `inputAttemptWhenModal`).
2. The window that holds the editor stops being the key window (`juce_NSViewComponentPeer_mac.mm`: `resignKey()` -> `sendModalInputAttemptIfBlocked(KeyWindowChanged::yes)` -> `MenuWindow::inputAttemptWhenModal()`). A menu window can never be key itself (`canBecomeKeyWindow()` is false with `windowIgnoresKeyPresses`), so the test "is the modal window key" never protects it. With no target component, `inputAttemptWhenModal()` calls `dismissMenu(nullptr)` at once.
3. The host window moves or is minimized (`dismissModals()`).
4. The menu's own 20 Hz timer (`juce_PopupMenu.cpp`, `MouseSourceState::checkButtonState`): `if (! window.doesAnyJuceCompHaveFocus() && ! reallyContained)` and more than 10 ms since the last "focused" tick -> `dismissMenu(nullptr)`. `doesAnyJuceCompHaveFocus()` starts with `isForegroundOrEmbeddedProcess()`, which on macOS is `Process::isForegroundProcess()` = `[NSApp isActive]` (`juce_Threads_mac.mm`). If that is false, the menu closes at the first tick, 50 ms after it opened, unless the pointer is already over the menu. If it is true, the function returns true when any JUCE component has the keyboard focus, and otherwise `! hasAnyJuceCompHadFocus`, a flag that is set only on a path that is skipped while a component has the focus: so with the application "active", it returns true, the menu is "focused", and the timer never closes it.

A press in a window of the host is none of JUCE's business. So for a click away into the host, only ways 2 and 4 are left, and both read state that the host hands across the process boundary (is the helper the active application, is the plug-in's window key), not the mouse.

- **The menu flashes and closes** = way 4 with `[NSApp isActive]` false in the helper at the first tick (the pointer is on the control, not on the menu, so `reallyContained` is false), or way 2 when the key status moves just after the menu opened (the editor's `mouseDown` called `grabKeyboardFocus()`, which asks for key status, and then showed the menu). Measured for the first: the same `juce::PopupMenu`, shown the same way in a process that is not the active application, closed itself after 71 ms with no input (`KindredUiTest --legacy`).
- **The menu stays** = the helper counts as active and its window state does not change when the click lands in another window of Logic: way 2 has nothing to fire on, way 4 returns "focused" on every tick, ways 1 and 3 do not apply. The menu waits for a press inside the editor.
- **It works** = the helper is active and the click away does move the key status, so way 2 fires.

Which of the three happens depends on the activation and key state that Logic gives the helper at that moment, which is why the fault came and went. What was measured: where the plug-in runs, and the self-closing popup. What is reasoning from the JUCE source: the mapping of each symptom to a way. Nobody recorded `[NSApp isActive]` or the key window inside the helper while the fault happened.

**Fix: no popup windows.** Not a repair of the popup's settings, because any top-level popup keeps the dependency on state from the host.

- `KindredEditor::showMenu` still takes a `juce::PopupMenu`, but only as a list of rows. The editor draws the menu itself (`paintMenu`), in its own view, on top of everything else, and handles it in its own mouse and key functions (`menuHit`, `menuHover`, `menuChoose`, `menuKey`). Nothing is modal. No callback outlives the editor (the chosen action runs inside the editor's own `mouseUp`), so nothing can dangle.
- A press on a row and a release on it chooses. Press on the control, drag, release on a row also chooses. The release of the click that opened the menu chooses nothing unless the pointer moved more than 6 px or 300 ms passed.
- A press anywhere else in the editor closes the menu and does nothing else (so the second click on the same control closes it and cannot open it again).
- A press outside the editor (another window of the host, the desktop, another application): the editor's 60 Hz timer looks for it while a menu or the panel is open (`watchOutsidePress`, rule in `pressWasElsewhere`). The question it asks is "did the system see a mouse press that none of our components received?". The system side: the button state (`ComponentPeer::getCurrentModifiersRealtime`) and, on macOS, the time of the last button-down in the login session (`CGEventSourceSecondsSinceLastEventType`, which also catches a press shorter than one timer period). Our side: a global mouse listener that notes every mouse-down JUCE delivers. It uses no focus state, no key window, no application state and no screen coordinates, because in a helper process none of them is ours.
- Escape, a resize of the editor, hiding or closing the editor all close it.
- The performance panel is a child component of the editor (was a `CallOutBox`, a second kind of popup window). Rename is a text field over the tab (was a modal `AlertWindow`). They close by the same rules. The file dialogs are the system's own and are unchanged.

**What only a person in Logic can confirm.** (1) That the two symptoms are gone: open each menu many times, click away into the Logic main window, the mixer, another plug-in window and the desktop. (2) That a press outside the editor closes the menu there. The rule is tested with numbers (12 cases), but its two inputs from the system were never read inside Logic's sandboxed helper; if the helper gets no answer, a menu stays until a click in the editor or Escape, and never closes by itself. (3) That Logic delivers mouse-move events to the editor while no button is down (the row under the pointer lights up); if not, rows still work by click. (4) That the keys reach the editor in Logic (Escape, arrows): Logic can keep key presses for itself, and then only the mouse closes a menu. (5) How it looks.

**Build 2 and a running Logic.** Build 2 was installed while Logic Pro was running with build 1 loaded. The running helper keeps build 1 (the install writes new files and does not touch the loaded ones). Logic loads build 2 after Logic is quit and opened again.

## Portability (2026-11-13)

**macOS.** Deployment target 10.13, architectures `arm64;x86_64` (`CMakeLists.txt`). JUCE 8.0.8 itself supports a deployment target of 10.11 for every format (its README: "macOS 10.11 (x86_64, Arm64)"), so nothing in JUCE had to change. The whole build, JUCE included, compiles with `-Werror=unguarded-availability-new -Werror=unguarded-availability`: 0 errors. Kindred's own code uses no `std::filesystem`, no `std::variant`, no `std::to_chars`, no `std::any`; it uses `std::optional` with `*` and `->` only (no `.value()`). The one system function it calls directly, `CGEventSourceSecondsSinceLastEventType`, exists since 10.4.

What the bundles say (`tools/minos.sh`, which reads `vtool -show-build` and the Info.plist; `tests/out/minos.log`):

| Bundle | x86_64 | arm64 | LSMinimumSystemVersion |
|---|---|---|---|
| `Kindred.component` (AU) | 10.13 | 11.0 | 10.13 |
| `Kindred.vst3` | 10.13 | 11.0 | 10.13 |
| `Kindred.app` (Standalone) | 10.13 | 11.0 | 10.13 |

The arm64 half says 11.0 because no older macOS runs on Apple silicon; the linker never writes less. The x86_64 half links only libraries that 10.13 has (Metal and MetalKit are linked weak).

Tests of the Intel half, under Rosetta on this Mac: golden equivalence 80,894 comparisons, 0 mismatches; host test 40 checks, 0 failed; UI test 625 steps, 0 failed; Standalone 7 s without a crash. **Not done: a real Intel Mac, and a real macOS 10.13, 10.14 or 10.15.** The deployment target and the availability check say that it should load there; nothing ran there. **`auval` for the Intel half did not run:** `arch -x86_64 auval -v aumi Kndr Nglw` answers "didn't find the component" (two tries), and `arch -x86_64 auval -a` did not finish in 7 minutes and was stopped. The Intel-side component list of this Mac does not have Kindred; the reason was not found. The arm64 `auval` passes, and the Intel VST3 passes the host test.

**Windows.** Prepared, not built: there is no Windows machine here, and no MSVC. Changes:

- `CMakeLists.txt`: `/utf-8` (the sources have note names with flat and sharp signs and other UTF-8 text in plain string literals), `/fp:strict` for the engine in place of `-ffp-contract=off`, `/W4` in place of `-Wall -Wextra`, the static C++ runtime (no redistributable needed), `_CRT_SECURE_NO_WARNINGS`.
- `cmake/Plugin.cmake`: `JUCE_DIR` is a cache variable with a search order, formats are VST3 and Standalone outside macOS (no AU), the host test asks for AU hosting only on macOS.
- Every header that the engine and the tests use is now included by name (libc++ pulls in headers that the MSVC library does not: `<stdexcept>`, `<cstdlib>`, `<functional>`, `<utility>`, `<string>`).
- The user data folder is `%APPDATA%\Neglectware\Kindred` on Windows (was a Mac path below the roaming folder). The built-in styles and the fonts are compiled in (`juce_add_binary_data`), so there is no file to find at run time.
- The one Mac-only call in the sources is behind `#if JUCE_MAC`. On Windows a press outside the editor is found by the button state alone.
- `.gitattributes` keeps LF line ends, so the compiled-in style files are the same bytes on every system.
- `.github/workflows/build.yml`: Windows x64 zip (VST3 folder, `Kindred.exe`, `INSTALL-Windows.txt`, the manual), then the engine tests and the host test on Windows; macOS universal bundles, the minimum-version check, the engine tests (also as x86_64 when the runner can), the host test and the UI test. JUCE is cloned at tag 8.0.8. Nothing is published: artifacts only.

**Unverified until the workflow runs:** that MSVC compiles the sources at all; that the golden test passes on Windows (the engine calls `std::pow` in one place, with a small whole-number exponent; the result must equal V8's to the last bit, and it does on both Mac architectures, but the Windows C library is a third implementation); that the VST3 loads; that the zip step finds the files where the Visual Studio generator puts them; that the macOS runner's Xcode accepts a 10.13 target; that the UI test runs on a runner without a display session.

## Window size fault (2026-11-13, build 3)

Report: the editor opened smaller than expected on a 4K display (macOS "looks like 3200 x 1800"). It was the minimum size, 840 points wide, not a scaling fault: the UI test and the screenshot tool resize editors and the editor saves its width to the user settings file (`editorW`), so the tools left 840 there and every new editor opened at 840. Fix: the tools set `KINDRED_SETTINGS_FILE` to a scratch file before they start, `KindredProcessor::settings()` uses that file when the variable is set, and `tools/build.sh` fails if the UI test changes the user's settings file. A user who already has 840 saved drags the corner once; the new width is saved.
