"use strict";
// node tools/make-style.js
// Writes styles/warm-ambient.json from the prototype's tables (tests/golden/prototype/engine-v2.js),
// so the first style is a transcription by program, not by hand.
const path = require("path"), fs = require("fs");
const E = require(path.join(__dirname, "..", "tests", "golden", "prototype", "engine-v2.js"));

const style = {
  format: 1,
  id: "warm-ambient",
  name: "Warm Ambient",
  description: "Slow, connected chords over a bass that drones and moves. Small diatonic vocabulary, plagal motion, add9 and sus colors, every change keeps a shared note.",
  defaults: { mode: "ionian", tempo: 60, bpb: 4, tie: true, sections: ["home", "lift", "away", "return"],
    arrangement: [[0, 16], [1, 8], [0, 8], [2, 8], [1, 8], [0, 8], [3, 8]] },
  modes: E.MODE_ORDER.map(id => {
    const m = { id, name: E.MODES[id].name, steps: E.MODES[id].steps, minor: E.MODES[id].minor };
    m.borrow = id === "ionian" ? ["iv", "bVII"] : id === "mixolydian" ? ["iv"] : [];
    if (id === "lydian") m.planBias = { moving: 0.4, md: 0.8, "*": 1.4 };      // Lydian needs its tonic underneath
    return m;
  }),
  spectra: [
    { id: "wistful",  name: "Brightness",     low: "Bright", high: "Wistful",  lowHint: "major chords",   highHint: "more minor chords",              min: 0, max: 1, default: 0.35 },
    { id: "moving",   name: "Bass motion",    low: "Drone",  high: "Moving",   lowHint: "the bass holds", highHint: "the bass follows the chords",    min: 0, max: 1, default: 0.5 },
    { id: "color",    name: "Color",          low: "Plain",  high: "Rich",     lowHint: "triads",         highHint: "add9, maj7, sus",                min: 0, max: 1, default: 0.5 },
    { id: "inner",    name: "Inner movement", low: "Still",  high: "Flowing",  lowHint: "none",           highHint: "suspensions and passing notes",  min: 0, max: 1, default: 0.5 },
    { id: "surprise", name: "Surprise",       low: "In key", high: "Borrowed", lowHint: "diatonic only",  highHint: "one borrowed chord",             min: 0, max: 1, default: 0.2 },
    { id: "leadins",  name: "Lead-ins",       low: "None",   high: "Often",    lowHint: "no approach chords", highHint: "a short chord before a change", min: 0, max: 1, default: 0 },
    { id: "edge",     name: "Edge",           low: "Pretty", high: "Strange",  lowHint: "every change shares a note", highHint: "extended colors, distant chords", min: 0, max: 1, default: 0 },
  ],
  extended: { name: "Extended colors", hint: "6, 6/9, maj9, m9, m11, ♯11" },
  extensions: E.EXTS,
  colorWeights: {
    plain:   { base: 0.35, plain: 1.5, degree: { "7maj": 0.6 }, degreeOwnRootOnly: true },
    add9:    { color: 1.3, overDrone: 0.3 },
    sus2:    { color: 0.5 },
    maj7:    { color: 0.8, degree: { "5": 1.3 }, degreeDefault: 0.8, overDrone: 0.3 },
    sus4:    { base: 0.15, color: 1.6 },
    m7:      { color: 1.1 },
    madd9:   { color: 0.25 },
    default: { color: 0.35 },
  },
  tokens: E.TOK,
  cells: E.CELLS,
  plans: [
    { id: "drone",  label: E.PLANS.drone.label,  pattern: { all: "T" } },
    { id: "dm",     label: E.PLANS.dm.label,     pattern: { half: ["T", "R"] } },
    { id: "end",    label: E.PLANS.end.label,    pattern: { tail: ["T", "R"], long: 2, short: 1 } },
    { id: "md",     label: E.PLANS.md.label,     pattern: { half: ["R", "T"] } },
    { id: "moving", label: E.PLANS.moving.label, pattern: { all: "R" } },
    { id: "fifth",  label: E.PLANS.fifth.label,  pattern: { all: "F" } },
  ],
  // planWeights: a sum of terms [c, d, m] = c * D^d * M^m, where M is the Bass motion control and D = 1 - M.
  roles: [
    { id: "home", name: "Verse", length: 8, plans: ["drone", "dm", "end", "md", "moving"],
      planWeights: { drone: [[1.3, 2, 0], [0.05, 0, 0]], dm: [[0.5, 0, 0], [2.4, 1, 1]], end: [[0.3, 0, 0], [1.6, 1, 1]], md: [[0.15, 0, 0], [0.8, 1, 1]], moving: [[1.3, 0, 2], [0.05, 0, 0]] } },
    { id: "lift", name: "Chorus", length: 8, plans: ["drone", "dm", "end", "md", "moving"],
      planWeights: { drone: [[0.5, 2, 0]], dm: [[0.1, 0, 0], [0.4, 1, 0]], end: [[0.1, 0, 0], [0.5, 1, 0]], md: [[0.3, 0, 0], [1.6, 1, 1]], moving: [[0.4, 0, 0], [1.3, 0, 1]] } },
    { id: "away", name: "Middle eight", length: 8, plans: ["fifth", "moving", "md", "dm", "drone"],
      planWeights: { fifth: [[0.5, 0, 0], [1.2, 1, 0]], moving: [[0.3, 0, 0], [1, 0, 1]], md: [[0.5, 0, 0]], dm: [[0.3, 0, 0]], drone: [[0.3, 1, 0]] } },
    { id: "turn", name: "Turnaround", length: 4, plans: ["drone", "dm", "end", "md", "moving"],
      planWeights: { drone: [[0.3, 1, 0]], dm: [[0.3, 0, 0]], end: [[0.6, 0, 0]], md: [[0.2, 0, 0]], moving: [[0.5, 0, 0], [1, 0, 1]] } },
    { id: "return", name: "Coda", length: 8, plans: ["drone", "md"],
      planWeights: { drone: [[1.2, 0, 0], [1, 1, 0]], md: [[0.4, 0, 0], [1, 0, 1]] } },
  ],
  tension: { "0": 0, "9": 0.3, "4": 0.35, "3": 0.35, "5": 0.4, "8": 0.45, "2": 0.5, "11": 0.5, "10": 0.55, "7": 0.7, default: 0.5 },
  drone: { tonicExcludes: [9, 4], fifthAllows: [0, 5, 2, 7, 10] },
  leadIns: [
    { w: 1.0, offset: -1, q: "maj", ext: "", iv: [0, 4, 7], sharp: true },                                  // a half step below
    { w: 0.9, offset: 5, q: "target", ext: "", iv: [0, 4, 7], extMinor: "m", ivMinor: [0, 3, 7] },          // its IV (plagal)
    { w: 0.7, offset: 7, q: "maj", ext: "sus4", iv: [0, 5, 7] },                                            // its V, suspended
  ],
  edge: { borrow: [[8, "maj"], [3, "maj"], [4, "maj"], [9, "maj"], [2, "maj"], [7, "min"], [5, "maj"], [1, "maj"]] },
  rules: {
    voiceLo: 50, voiceHi: 79, voiceCenter: 63, voiceMaxSpan: 19, voiceLowLimit: 57, voiceLowMinGap: 5, bassLo: 33, bassHi: 45,
    maxDistinct: 4, borrowChance: 0.55, borrowChanceAway: 0.9, cellDriveSlope: 1.6, cellDrivePivot: 0.4, cellMinWeight: 0.15,
    minorBase: 0.15, minorSlope: 2.2, pedalMisfit: 0.3, lockedMatch: 8, lockedMiss: 0.5, homePlanRepeat: 0.35, connectBoost: 1.2,
    susCapBase: 0.15, susCapSlope: 0.35, susReleaseDrop: 0.15, susLastBase: 0.4, susLastSlope: 0.6, susReleaseMul: 1.2, susReleaseMax: 0.9,
    susOtherMul: 0.2, susRepeatMax: 2, tailChance: 0.6, chordVelocity: 72, bassVelocity: 80,
    edgeColorWeight: 1.1, edgeBorrowChance: 0.8, edgeDisconnect: 0.5, edgeMaxBorrowed: 2,
    seamKeepsNeighbor: true,
  },
};

// Compact but readable: one line per leaf list.
function fmt(v, ind = 0) {
  const pad = "  ".repeat(ind);
  const leaf = x => x === null || typeof x !== "object" || (Array.isArray(x) && x.every(y => y === null || typeof y !== "object" || (Array.isArray(y) && y.every(z => typeof z !== "object"))) && JSON.stringify(x).length < 150)
    || (!Array.isArray(x) && Object.values(x).every(y => y === null || typeof y !== "object" || (Array.isArray(y) && y.every(z => typeof z !== "object"))) && JSON.stringify(x).length < 150);
  if (leaf(v)) return JSON.stringify(v, null, 1).replace(/\n\s*/g, " ").replace(/([\[{]) /g, "$1").replace(/ ([\]}])/g, "$1");
  if (Array.isArray(v)) return "[\n" + v.map(x => pad + "  " + fmt(x, ind + 1)).join(",\n") + "\n" + pad + "]";
  return "{\n" + Object.entries(v).map(([k, x]) => pad + "  " + JSON.stringify(k) + ": " + fmt(x, ind + 1)).join(",\n") + "\n" + pad + "}";
}
const out = path.join(__dirname, "..", "styles", "warm-ambient.json");
const text = fmt(style) + "\n";
JSON.parse(text);
fs.writeFileSync(out, text);
console.log("wrote", out, text.length, "bytes");

// ---- A second style, to show that a style is data. Same engine, same tables, other weights,
// ---- ranges, names and defaults. Nobody has tuned it by ear: it is an example, not a finished genre.
const road = JSON.parse(JSON.stringify(style));
Object.assign(road, { id: "open-road", name: "Open Road (example)",
  description: "An example second style: pop and rock motion. The bass follows the chords, the chords are plainer and are struck again at each change, and functional progressions (I V vi IV) are favored over rocking in place." });
road.defaults = { mode: "ionian", tempo: 96, bpb: 4, tie: false, sections: ["home", "lift", "away", "turn", "return"],
  arrangement: [[0, 8], [1, 8], [0, 8], [1, 8], [2, 8], [3, 4], [1, 8], [4, 8]] };
road.modes = road.modes.filter(m => ["ionian", "mixolydian", "aeolian"].includes(m.id));
for (const m of Object.keys(road.cells)) if (!road.modes.some(x => x.id === m)) delete road.cells[m];
// Favor cells with drive: weight * (0.4 + 1.6 * drive).
for (const m of Object.values(road.cells)) for (const list of Object.values(m)) for (const c of list) c[1] = Math.round(c[1] * (0.4 + 1.6 * c[2]) * 100) / 100 || 0.05;
const spec = id => road.spectra.find(x => x.id === id);
Object.assign(spec("moving"), { min: 0.4, max: 1, default: 0.9, lowHint: "some held bass notes" });
Object.assign(spec("color"), { default: 0.25 });
Object.assign(spec("inner"), { max: 0.6, default: 0.15 });
Object.assign(spec("wistful"), { default: 0.45 });
Object.assign(spec("leadins"), { default: 0.1 });
const names = { home: "Verse", lift: "Chorus", away: "Bridge", turn: "Turnaround", return: "Outro" };
for (const r of road.roles) r.name = names[r.id];
road.roles.find(r => r.id === "away").plans = ["moving", "md", "dm", "fifth"];
road.rules.maxDistinct = 4; road.rules.chordVelocity = 84; road.rules.bassVelocity = 92; road.rules.cellDriveSlope = 2.2;
const out2 = path.join(__dirname, "..", "styles", "open-road.json");
const text2 = fmt(road) + "\n"; JSON.parse(text2); fs.writeFileSync(out2, text2);
console.log("wrote", out2, text2.length, "bytes");
