"use strict";
// ---------------------------------------------------------------- engine ----
// v2. A section is a phrase built in layers:
//   1. a skeleton of known four-chord cells (the second half of a phrase starts
//      like the first and ends differently),
//   2. a bass plan: where the bass sits on a drone (tonic, or the fifth) and
//      where it leaves to follow the chord roots,
//   3. color: an added note is chosen because a neighboring chord also has it,
//   4. voicing with common tones kept in place,
//   5. inner movement: a note held over a change that then resolves, or one
//      voice stepping toward the next chord while the others hold.
// Every roll is stored as explicit notes, bass notes and beats, never a seed.
const ENGINE_VERSION = "2.0.0";
const SCHEMA = 3;
const PPQ = 960;
const MODES = {
  lydian:     { name: "Lydian",     steps: [0,2,4,6,7,9,11], minor: false },
  ionian:     { name: "Major",      steps: [0,2,4,5,7,9,11], minor: false },
  mixolydian: { name: "Mixolydian", steps: [0,2,4,5,7,9,10], minor: false },
  dorian:     { name: "Dorian",     steps: [0,2,3,5,7,9,10], minor: true },
  aeolian:    { name: "Minor",      steps: [0,2,3,5,7,8,10], minor: true },
};
const MODE_ORDER = ["lydian","ionian","mixolydian","dorian","aeolian"];
const SHARP = ["C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"];
const FLAT  = ["C","D♭","D","E♭","E","F","G♭","G","A♭","A","B♭","B"];
const FLAT_KEYS = new Set([5,10,3,8,1,6]);
const mod12 = n => ((n % 12) + 12) % 12;

function makeRng(seed) {
  let a = seed >>> 0;
  return () => { a = (a + 0x6D2B79F5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
}
function pickWeighted(items, weights, rand) {
  const sum = weights.reduce((a, b) => a + Math.max(0, b), 0);
  if (sum <= 0) return items[Math.floor(rand() * items.length)];
  let r = rand() * sum;
  for (let i = 0; i < items.length; i++) { r -= Math.max(0, weights[i]); if (r <= 0) return items[i]; }
  return items[items.length - 1];
}

const TRIAD = { maj: [0,4,7], min: [0,3,7], dim: [0,3,6], aug: [0,4,8] };
// Warm palette. "jazz" entries only appear with the Jazz colors switch.
// There is no plain added sixth and no dominant seventh in the warm palette.
const EXTS = {
  maj: [ {s:"",iv:[0,4,7],t:"plain"}, {s:"add9",iv:[0,4,7,14],t:"add9"}, {s:"sus2",iv:[0,2,7],t:"sus2"},
         {s:"maj7",iv:[0,4,7,11],t:"maj7"}, {s:"sus4",iv:[0,5,7],t:"sus4"},
         {s:"6",iv:[0,4,7,9],t:"jazz"}, {s:"6/9",iv:[0,4,7,9,14],t:"jazz"}, {s:"maj9",iv:[0,4,7,11,14],t:"jazz"},
         {s:"maj7♯11",iv:[0,4,7,11,18],t:"jazz"}, {s:"9sus4",iv:[0,5,7,10,14],t:"jazz"} ],
  min: [ {s:"m",iv:[0,3,7],t:"plain"}, {s:"m7",iv:[0,3,7,10],t:"m7"}, {s:"m(add9)",iv:[0,3,7,14],t:"madd9"},
         {s:"m9",iv:[0,3,7,10,14],t:"jazz"}, {s:"m11",iv:[0,3,7,10,14,17],t:"jazz"}, {s:"m6",iv:[0,3,7,9],t:"jazz"} ],
  dim: [ {s:"°",iv:[0,3,6],t:"plain"} ],
};

const TOK = { I:[0,"maj"], i:[0,"min"], ii:[2,"min"], II:[2,"maj"], bIII:[3,"maj"], iii:[4,"min"], IV:[5,"maj"], iv:[5,"min"],
  V:[7,"maj"], v:[7,"min"], bVI:[8,"maj"], vi:[9,"min"], bVII:[10,"maj"], vii:[11,"min"] };
// Four-chord cells: [chords, weight, drive]. "drive" 0 = rocking in place, 1 = a
// full functional circle. Every cell is a progression in long common use.
//   home  starts on the tonic          lift  starts off the tonic (chorus)
//   away  middle eight, moving bass    ped5  middle eight, chords over the fifth
//   close the last four chords of a coda (always ends on the tonic)
const CELLS = {
  ionian: {
    home: [["I IV I IV",1.5,0],["I IV I V",0.9,.5],["I I IV I",0.8,0],["I IV I I",0.6,0],["I vi IV I",1.1,.3],["I vi IV V",0.9,.6],
      ["I vi IV IV",0.8,.3],["I iii IV IV",0.45,.4],["I iii IV V",0.35,.6],["I ii IV I",0.7,.4],["I ii I ii",0.5,0],["I ii I IV",0.6,.1],
      ["I ii IV V",0.45,.7],["I V vi IV",0.7,.9],["I IV vi IV",1.0,.3],["I vi I IV",0.6,.2],["I V IV I",0.6,.5],["I V IV IV",0.6,.5],
      ["I V I IV",0.7,.2],["I IV V IV",0.5,.7],["I vi ii V",0.3,1],["I IV ii V",0.4,.8]],
    lift: [["IV I IV I",1.2,0],["IV I IV V",1.2,.4],["IV I ii V",0.4,.7],["IV I vi V",0.6,.6],["IV vi I V",0.5,.6],["IV vi IV V",0.5,.5],
      ["vi IV I V",1.2,.7],["vi IV I I",0.9,.3],["vi IV I IV",0.8,.3],["vi I IV V",0.5,.6],["vi IV ii V",0.3,.9],
      ["ii IV I I",0.7,.3],["ii IV I V",0.7,.6],["ii vi IV I",0.4,.5],["IV V I I",0.4,.8],["IV V vi IV",0.4,.9]],
    away: [["vi ii IV V",0.6,.8],["vi IV ii V",0.5,.8],["vi iii IV I",0.6,.4],["ii vi IV V",0.5,.7],["vi IV vi IV",0.7,0],
      ["IV iii ii I",0.6,.4],["vi V IV V",0.5,.6],["ii iii IV V",0.35,.8],["vi vi IV IV",0.6,0],["ii IV vi V",0.4,.6],["vi IV I V",0.6,.6]],
    ped5: [["IV I IV V",1.2,0],["I IV I V",1.0,0],["ii I IV V",0.7,0],["IV I ii V",0.7,0],["I ii I V",0.6,0],["IV I IV I",0.8,0],["I IV I IV",0.6,0]],
    close: [["IV I IV I",1.2,0],["I IV I I",1.0,0],["ii IV I I",0.6,0],["vi IV I I",0.7,0],["IV IV I I",0.7,0],["V IV I I",0.5,0]],
  },
  lydian: {
    home: [["I II I II",1.6,0],["I I II I",1.0,0],["I II I I",0.8,0],["I vii I II",0.6,0],["I II vii I",0.6,.3],["I vi II I",0.5,.5],
      ["I V II I",0.4,.6],["I II I vii",0.6,.1],["I II vi II",0.4,.5],["I II I V",0.4,.4]],
    lift: [["II I II I",1.2,0],["vi II I I",0.6,.5],["II vii I I",0.6,.3],["V II I I",0.5,.6],["II I vi II",0.4,.5],["vii I II I",0.6,.1],["II I II vii",0.5,.2]],
    away: [["vi II vi II",0.7,.2],["vii vi II I",0.4,.6],["vi vii I II",0.5,.5],["II vii vi II",0.4,.5],["vii I vii II",0.6,.1],["vi vi II II",0.5,0]],
    ped5: [],
    close: [["II I II I",1.2,0],["I II I I",1.0,0],["vii II I I",0.5,0],["vi II I I",0.6,0],["II II I I",0.6,0]],
  },
  mixolydian: {
    home: [["I bVII IV I",1.5,.4],["I bVII I bVII",1.2,0],["I bVII IV IV",0.8,.4],["I IV I bVII",0.9,.1],["I bVII I IV",0.9,.1],
      ["I v IV I",0.6,.5],["I I bVII I",0.8,0],["I IV bVII I",0.8,.4],["I IV bVII IV",0.7,.4],["I ii bVII I",0.4,.5],["I IV I IV",0.7,0]],
    lift: [["bVII IV I I",1.2,.4],["IV I bVII I",0.9,.2],["IV bVII I I",0.8,.4],["v IV I I",0.6,.5],["bVII IV bVII I",0.7,.3],["IV I IV bVII",0.6,.2],
      ["bVII I bVII IV",0.6,.2],["vi IV bVII I",0.4,.7]],
    away: [["v IV v IV",0.6,.2],["ii IV bVII I",0.5,.6],["IV bVII IV v",0.5,.4],["vi IV bVII bVII",0.4,.5],["ii bVII IV IV",0.5,.5],["v bVII IV I",0.5,.6]],
    ped5: [["IV I IV I",1.0,0],["IV I bVII IV",0.8,0],["I IV I IV",0.8,0],["bVII I IV IV",0.6,0]],
    close: [["bVII IV I I",1.2,0],["IV I bVII I",0.8,0],["I bVII I I",0.8,0],["IV I IV I",0.8,0],["bVII I bVII I",0.7,0]],
  },
  dorian: {
    home: [["i IV i IV",1.5,0],["i bVII IV IV",0.8,.4],["i bIII IV i",0.7,.4],["i IV bVII i",0.7,.4],["i i IV i",0.8,0],["i v IV i",0.5,.5],["i IV i bVII",0.7,.1]],
    lift: [["IV i IV i",1.2,0],["bIII IV i i",0.8,.4],["bVII IV i i",0.9,.4],["IV bVII i i",0.7,.4]],
    away: [["bIII bVII IV i",0.6,.6],["v IV v IV",0.6,.2],["bVII bIII IV IV",0.5,.5],["ii IV ii IV",0.4,.2]],
    ped5: [],
    close: [["IV i IV i",1.2,0],["i IV i i",1.0,0],["bVII IV i i",0.8,0]],
  },
  aeolian: {
    home: [["i bVI bIII bVII",1.1,.7],["i bVI i bVI",1.0,0],["i bVII bVI bVII",0.9,.4],["i bVI bVII i",0.8,.4],["i i bVI bVII",0.7,.2],["i iv i iv",0.6,0],["i bVI iv i",0.6,.4]],
    lift: [["bVI bVII i i",1.2,.4],["bVI bIII bVII i",1.0,.7],["bIII bVII i i",0.7,.4],["iv i bVI bVII",0.6,.4]],
    away: [["bIII bVII bVI bVII",0.7,.5],["iv bVI bIII bVII",0.6,.7],["bVI bVI bVII bVII",0.6,.1]],
    ped5: [],
    close: [["bVI bVII i i",1.2,0],["iv i iv i",0.8,0],["i bVI i i",0.8,0],["bVI iv i i",0.6,0]],
  },
};
// How the bass behaves across a section. T = tonic drone, F = drone on the
// fifth, R = the bass follows the chord roots.
const PLANS = {
  drone:  { label: "drone throughout",                     types: n => Array(n).fill("T") },
  dm:     { label: "drone, then the bass starts to move",  types: n => Array.from({ length: n }, (_, i) => i < n / 2 ? "T" : "R") },
  end:    { label: "drone, the bass moves at the end",     types: n => Array.from({ length: n }, (_, i) => i < n - (n > 4 ? 2 : 1) ? "T" : "R") },
  md:     { label: "moving bass that settles on a drone",  types: n => Array.from({ length: n }, (_, i) => i < n / 2 ? "R" : "T") },
  moving: { label: "moving bass",                          types: n => Array(n).fill("R") },
  fifth:  { label: "held over the fifth, waiting to come home", types: n => Array(n).fill("F") },
};
const PLAN_ORDER = ["drone", "dm", "end", "md", "moving", "fifth"];
const ROLES = ["home", "lift", "away", "turn", "return"];
const ROMAN = ["I","♭II","II","♭III","III","IV","♯IV","V","♭VI","VI","♭VII","VII"];

const parseTok = s => { const t = TOK[s]; return { deg: t[0], q: t[1] }; };
const ckey = c => `${c.deg}${c.q}`;
function triadQuality(r, t, f) {
  const a = mod12(t - r), b = mod12(f - r);
  if (a === 4 && b === 7) return "maj"; if (a === 3 && b === 7) return "min";
  if (a === 3 && b === 6) return "dim"; if (a === 4 && b === 8) return "aug"; return null;
}
function diatonicTriads(steps) {
  return steps.map((deg, d) => ({ deg, q: triadQuality(deg, steps[(d+2)%7], steps[(d+4)%7]) }));
}
const isDiatonic = (mode, c) => diatonicTriads(MODES[mode].steps).some(t => t.deg === c.deg && t.q === c.q);
function parentScale(mode, c) {
  for (const id of [mode, ...MODE_ORDER]) if (isDiatonic(id, c)) return MODES[id].steps;
  return null;
}
const triadPcs = c => TRIAD[c.q].map(i => mod12(c.deg + i));
function commonTones(a, b) { const A = new Set(triadPcs(a)); return triadPcs(b).filter(p => A.has(p)).length; }
const fifthDeg = 7;
// A chord over a drone it does not contain the root of. vi over the tonic is an
// added-sixth chord and iii over the tonic hides the bass move, so those two
// take their own root instead (the drone lets go for that chord).
function pedalOk(c, bt) {
  if (bt === "T") return c.deg !== 9 && c.deg !== 4;
  if (bt === "F") return [0, 5, 2, 7, 10].includes(c.deg);
  return true;
}
const bassDeg = (c, bt) => bt === "T" && pedalOk(c, bt) ? 0 : bt === "F" && pedalOk(c, bt) ? fifthDeg : c.deg;
const TENSION = { 0: 0, 9: .3, 4: .35, 3: .35, 5: .4, 8: .45, 2: .5, 11: .5, 10: .55, 7: .7 };
function tension(c, bdeg) {
  const t = TENSION[c.deg] ?? .5;
  if (bdeg === c.deg) return t;
  return bdeg === 0 ? t * 0.8 : Math.max(.6, t + .2);
}

// ---------------------------------------------------------------- names ----
function noteName(pc, keyPc, deg, preferSharp) {
  const flat = !preferSharp && (FLAT_KEYS.has(keyPc) || [1,3,8,10].includes(deg));
  return (flat ? FLAT : SHARP)[pc];
}
function chordName(ch, keyPc) {
  const name = noteName(ch.root, keyPc, mod12(ch.root - keyPc), ch.sharp) + ch.ext;
  if (ch.bass != null && mod12(ch.bass) !== ch.root) return name + "/" + noteName(mod12(ch.bass), keyPc, mod12(ch.bass - keyPc), false);
  return name;
}
function romanName(ch, keyPc) {
  const deg = mod12(ch.root - keyPc);
  let n = ROMAN[deg];
  if (ch.q === "min" || ch.q === "dim") n = n.toLowerCase();
  let s = ch.ext;
  if (ch.q === "min") s = s.replace(/^m/, "");
  if (ch.bass != null && mod12(ch.bass) !== ch.root) s += "/" + ROMAN[mod12(ch.bass - keyPc)];
  return n + s;
}
const keyName = k => FLAT_KEYS.has(k) ? FLAT[k] : SHARP[k];
const pcName = (m, keyPc) => noteName(mod12(m), keyPc ?? 0, mod12(m - (keyPc ?? 0)), false);
const degOf = (proj, ch) => mod12(ch.root - proj.key);
const asCand = (proj, ch) => ({ deg: degOf(proj, ch), q: ch.q in TRIAD ? ch.q : "maj", kind: ch.kind || "diatonic" });
const chordPcs = ch => { const s = new Set(ch.iv.map(i => mod12(ch.root + i))); if (ch.bass != null) s.add(mod12(ch.bass)); return s; };
const sharesTone = (a, b) => { const B = chordPcs(b); for (const p of chordPcs(a)) if (B.has(p)) return true; return false; };

// -------------------------------------------------------------- skeleton ----
function vocab(mode) {
  return diatonicTriads(MODES[mode].steps).filter(t => t.q === "maj" || t.q === "min");
}
function cellList(mode, kind) {
  return (CELLS[mode][kind] || []).map(([s, w, d]) => ({ id: s, w, d, chords: s.split(" ").map(parseTok) }));
}
function homeSection(proj, sec) {
  return proj.sections.find(s => s !== sec && s.role === "home" && s.slots.some(x => x.chord)) || null;
}
function allowedPlans(proj, role) {
  const fifthOk = CELLS[proj.mode].ped5.length > 0;
  if (role === "return") return ["drone", "md"];
  if (role === "away") return fifthOk ? ["fifth", "moving", "md", "dm", "drone"] : ["moving", "md", "dm", "drone"];
  return ["drone", "dm", "end", "md", "moving"];
}
function choosePlan(proj, sec, rand) {
  const ok = allowedPlans(proj, sec.role);
  if (sec.planPref && sec.planPref !== "auto" && ok.includes(sec.planPref)) return sec.planPref;
  const M = proj.mood.moving, D = 1 - M;
  const W = {
    home:   { drone: 1.3*D*D + .05, dm: .5 + 2.4*D*M, end: .3 + 1.6*D*M, md: .15 + .8*D*M, moving: 1.3*M*M + .05 },
    lift:   { drone: .5*D*D, dm: .1 + .4*D, end: .1 + .5*D, md: .3 + 1.6*D*M, moving: .4 + 1.3*M },
    turn:   { drone: .3*D, dm: .3, end: .6, md: .2, moving: .5 + M },
    away:   { fifth: .5 + 1.2*D, moving: .3 + M, md: .5, dm: .3, drone: .3*D },
    return: { drone: 1.2 + D, md: .4 + M },
  }[sec.role] || {};
  const home = homeSection(proj, sec);
  const w = ok.map(id => {
    let x = W[id] ?? .2;
    if (home && home.plan && home.plan.id === id && sec.role !== "return") x *= .35;
    if (id === "fifth") x *= 1.5 - proj.mood.wistful;
    if (proj.mode === "lydian") x *= id === "moving" ? .4 : id === "md" ? .8 : 1.4;   // Lydian needs its tonic underneath
    return x;
  });
  return pickWeighted(ok, w, rand);
}
function cellWeight(proj, cell, types, off, locked) {
  const m = proj.mood;
  let w = cell.w * Math.max(.15, 1 + (m.moving - .5) * 1.6 * (cell.d - .4));
  if (!MODES[proj.mode].minor) w *= Math.pow(.15 + 2.2 * m.wistful, cell.chords.filter(c => c.q === "min").length);
  cell.chords.forEach((c, k) => {
    const bt = types[off + k];
    if (bt !== "R" && !pedalOk(c, bt)) w *= .3;
    const L = locked[off + k];
    if (L) w *= ckey(L) === ckey(c) ? 8 : .5;
  });
  return w;
}
// The chords of a section before any color. `locked` holds the chords of locked slots.
function chooseSkeleton(proj, sec, n, planId, locked, rand) {
  const types = PLANS[planId].types(n), mode = proj.mode, role = sec.role;
  const home = homeSection(proj, sec);
  const homeSet = home ? new Set(home.slots.filter(x => x.chord).map(x => ckey(asCand(proj, x.chord)))) : null;
  const pick = (cells, off, extra) => pickWeighted(cells, cells.map(c => cellWeight(proj, c, types, off, locked) * (extra ? extra(c) : 1)), rand);
  const related = c => homeSet ? .3 + new Set(c.chords.map(ckey).filter(k => homeSet.has(k))).size : 1;
  // How closely a second half follows the first: same first two chords, same first chord, or neither.
  const near = (c, a) => ckey(c.chords[0]) !== ckey(a.chords[0]) ? (role === "home" ? 0 : .12) : ckey(c.chords[1]) === ckey(a.chords[1]) ? 1 : .3;
  const rocking = c => ckey(c.chords[0]) === ckey(c.chords[2]) && ckey(c.chords[1]) === ckey(c.chords[3]);
  const open = c => c.chords[3].deg !== 0;
  let kind = role === "home" ? "home" : role === "away" ? (planId === "fifth" ? "ped5" : "away") : role === "return" ? "close" : "lift";
  let cells = cellList(mode, kind), a, b, from = null;
  if (role === "turn") { let o = cells.filter(open); if (!o.length) o = cellList(mode, "away").filter(open); if (o.length) cells = o; }
  if (role === "return") {
    b = pick(cells, n - 4, related);
    if (n > 4) {
      const hs = home ? home.slots.slice(0, 4).filter(x => x.chord) : [];
      if (hs.length === 4) { a = { id: "home", chords: hs.map(x => asCand(proj, x.chord)) }; from = home.name; }
      else a = pick(cellList(mode, "home"), 0);
    }
    return { types, from, seq: [...(a ? a.chords : []), ...b.chords].map(c => ({ kind: "diatonic", ...c })) };
  }
  a = pick(cells, 0, role === "home" ? null : related);
  if (n > 4) {
    b = pick(cells, 4, c => {
      let x = near(c, a) * (c.id === a.id ? (c.chords[3].deg === 0 ? .25 : rocking(c) && role !== "home" ? .25 : .6) : 1);
      if (role === "lift" || role === "turn") x *= open(c) ? 1.3 : .4;       // a chorus hands back to the verse
      if (role === "away") x *= open(c) ? 1.5 : .5;
      return x;
    });
  }
  if (home && role !== "home") from = home.name;
  return { types, from, seq: [...a.chords, ...(b ? b.chords : [])].map(c => ({ ...c, kind: "diatonic" })) };
}
// At most one borrowed chord per section: the minor iv before I, or ♭VII in
// place of V or ii before I or IV.
function borrowOne(proj, sec, seq, types, isFixed, rand) {
  const m = proj.mood, n = seq.length;
  if (MODES[proj.mode].minor || proj.mode === "lydian" || seq.some(c => c.kind === "borrowed")) return;
  if (rand() >= m.surprise * (sec.role === "away" ? .9 : .55)) return;
  const opts = [];
  for (let j = sec.role === "return" ? Math.max(1, n - 4) : 1; j < n; j++) {
    if (isFixed(j) || types[j] === "F") continue;
    const c = seq[j], nx = j + 1 < n ? seq[j + 1] : (sec.role === "return" ? null : seq[0]), pv = seq[j - 1];
    if (!nx) continue;
    if (c.deg === 5 && c.q === "maj" && nx.deg === 0 && pv.deg !== 5) opts.push({ j, c: { deg: 5, q: "min", kind: "borrowed" }, w: (.3 + m.wistful) * (.2 + j / n) });   // best late in the phrase
    if (proj.mode === "ionian" && ((c.deg === 7 && c.q === "maj") || c.deg === 2) && (nx.deg === 0 || nx.deg === 5) && pv.deg !== 10)
      opts.push({ j, c: { deg: 10, q: "maj", kind: "borrowed" }, w: 1 });
  }
  if (!opts.length) return;
  const o = pickWeighted(opts, opts.map(x => x.w), rand);
  seq[o.j] = o.c;
}

// ------------------------------------------------------------------ color ----
function palette(proj, c, foreign, bdeg) {
  const scale = parentScale(proj.mode, c);
  const tri = TRIAD[c.q];
  return EXTS[c.q].filter(e => {
    if (c.kind === "borrowed" && e.t !== "plain" && !(e.t === "add9" && c.deg === 10)) return false;
    if (e.t === "jazz" && (!proj.jazz || foreign)) return false;
    if (scale && !e.iv.every(i => scale.includes(mod12(c.deg + i)))) return false;
    if (e.t === "sus4") return c.deg === 7 && !foreign;
    // Over a drone: plain, add9, or maj7 on IV, and the added note is never a sixth above the bass.
    if (foreign) return e.t === "plain" || ((e.t === "add9" || (e.t === "maj7" && c.deg === 5)) && !e.iv.some(i => !tri.includes(i) && mod12(c.deg + i - bdeg) === 9));
    return true;
  });
}
function typeWeight(e, c, r, foreign) {
  const f = foreign ? .3 : 1;
  switch (e.t) {
    case "plain": return (.35 + 1.5 * (1 - r)) * (c.deg === 7 && c.q === "maj" && !foreign ? .6 : 1);
    case "add9":  return 1.3 * r * f;
    case "sus2":  return .5 * r;
    case "maj7":  return .8 * r * (c.deg === 5 ? 1.3 : .8) * f;
    case "sus4":  return .15 + 1.6 * r;
    case "m7":    return 1.1 * r;
    case "madd9": return .25 * r;
    default:      return .35 * r;
  }
}
const extPcs = (c, e, bdeg) => { const s = new Set(e.iv.map(i => mod12(c.deg + i))); s.add(bdeg); return s; };
const overlap = (A, B) => { for (const p of A) if (B.has(p)) return true; return false; };
// Chooses one color per distinct chord in the section, favoring an added note
// that a neighbor also has, then makes sure every change shares a note.
// `given[i]` is the set of sounding pitch classes (relative to the key) of a
// slot that must not change; such slots get no entry in the result.
function colorize(proj, seq, bdegs, given, wrap, rand, restLast) {
  const n = seq.length, r = proj.mood.color, exts = new Array(n).fill(null);
  const foreign = i => bdegs[i] !== seq[i].deg;
  const baseSet = i => given[i] || new Set([...triadPcs(seq[i]), bdegs[i]]);
  const groups = {};
  for (let i = 0; i < n; i++) if (!given[i]) (groups[ckey(seq[i]) + (foreign(i) ? "/" + bdegs[i] : "")] ||= []).push(i);
  for (const idxs of Object.values(groups)) {
    const c = seq[idxs[0]], fo = foreign(idxs[0]), pal = palette(proj, c, fo, bdegs[idxs[0]]), tri = new Set(triadPcs(c));
    const w = pal.map(e => {
      let x = typeWeight(e, c, r, fo);
      const colors = e.iv.map(i => mod12(c.deg + i)).filter(p => !tri.has(p));
      let conn = 0;
      for (const i of idxs) for (const j of [i - 1, i + 1]) { if (j < 0 || j >= n) continue; const S = baseSet(j); if (colors.some(p => S.has(p))) conn++; }
      if (colors.length) x *= 1 + 1.2 * conn / (2 * idxs.length);     // an added note that a neighbor also has
      return x;
    });
    const e = pickWeighted(pal, w, rand);
    for (const i of idxs) exts[i] = e;
  }
  // The very last chord of a coda rests: a plain triad or add9.
  const L = n - 1;
  if (restLast && !given[L] && !["plain", "add9"].includes(exts[L].t)) {
    const pal = palette(proj, seq[L], false).filter(e => e.t === "plain" || e.t === "add9");
    exts[L] = pickWeighted(pal, pal.map(e => typeWeight(e, seq[L], r, false)), rand);
  }
  // The same chord twice in a row: one of the two changes color, so one voice moves.
  for (let i = 1; i < n; i++) {
    if (given[i] || given[i - 1] || ckey(seq[i]) !== ckey(seq[i - 1]) || bdegs[i] !== bdegs[i - 1] || exts[i] !== exts[i - 1]) continue;
    const k = restLast && i === L ? i - 1 : i;
    const pal = palette(proj, seq[k], foreign(k), bdegs[k]).filter(e => e !== exts[i] && e.t !== "jazz" && !(k > 0 && k < i && ckey(seq[k - 1]) === ckey(seq[k]) && e === exts[k - 1]));
    if (!pal.length || r <= 0) continue;
    exts[k] = pickWeighted(pal, pal.map(e => .2 + typeWeight(e, seq[k], Math.max(r, .3), foreign(k))), rand);
  }
  const sound = i => given[i] || extPcs(seq[i], exts[i], bdegs[i]);
  const fix = (i, j) => {          // make slot i share a note with slot j
    if (given[i] || (restLast && i === L)) return false;
    const S = sound(j), other = i > j ? i + 1 : i - 1;
    const keep = other >= 0 && other < n && (given[other] || overlap(sound(i), sound(other))) ? sound(other) : null;
    const pal = palette(proj, seq[i], foreign(i), bdegs[i]).filter(e => { const P = extPcs(seq[i], e, bdegs[i]); return overlap(P, S) && (!keep || overlap(P, keep)); });
    if (!pal.length) return false;
    exts[i] = pickWeighted(pal, pal.map(e => .1 + typeWeight(e, seq[i], .6, foreign(i))), rand);
    return true;
  };
  let ok = true;
  for (let i = 0; i + 1 < n; i++) if (!overlap(sound(i), sound(i + 1)) && !fix(i + 1, i) && !fix(i, i + 1)) ok = false;
  if (wrap && n > 1 && !overlap(sound(n - 1), sound(0))) fix(n - 1, 0) || fix(0, n - 1);
  return { exts, ok };
}

// ---------------------------------------------------------------- voicing ----
// Upper voices in D3..G5, moving as little as possible; a note two chords
// share stays where it is.
function voice(ch, prevNotes) {
  const pcs = [];
  ch.iv.forEach(i => { const p = mod12(ch.root + i); if (!pcs.includes(p)) pcs.push(p); });
  let upper = pcs.slice();
  if (upper.length >= 6) upper = upper.filter(p => p !== ch.root && p !== mod12(ch.root + 7));
  else if (upper.length === 5) upper = upper.filter(p => p !== ch.root);
  if (upper.length === 3) upper.push(ch.root);
  const LO = 50, HI = 79;
  const options = upper.map(pc => { const a = []; for (let m = LO; m <= HI; m++) if (mod12(m) === pc) a.push(m); return a; });
  for (const minGap of [2, 1]) {
    let best = null, bestCost = Infinity;
    const combo = new Array(upper.length);
    const rec = k => {
      if (k === upper.length) {
        const s = combo.slice().sort((a, b) => a - b);
        for (let i = 1; i < s.length; i++) { const g = s[i] - s[i-1]; if (g < minGap || (s[i] < 57 && g < 5)) return; }
        if (s[s.length-1] - s[0] > 19) return;
        const mean = s.reduce((a, b) => a + b, 0) / s.length;
        let cost;
        if (prevNotes && prevNotes.length) {
          const p = prevNotes, near = (x, arr) => Math.min(...arr.map(y => Math.abs(x - y)));
          cost = (s.reduce((a, x) => a + near(x, p), 0) + p.reduce((a, y) => a + near(y, s), 0)) / 2;
          const leap = Math.abs(s[s.length-1] - p[p.length-1]);
          cost += 0.9 * leap + (leap > 4 ? 2 : 0) + 0.12 * Math.abs(mean - 63);
        } else {
          cost = Math.abs(mean - 63);
          for (let i = 1; i < s.length; i++) if (s[i] - s[i-1] < 3) cost += 0.5;
        }
        for (let i = 2; i < s.length; i++) if (s[i] - s[i-2] <= 4) cost += 3;      // three notes inside a third
        if (cost < bestCost) { bestCost = cost; best = s; }
        return;
      }
      for (const m of options[k]) { combo[k] = m; rec(k + 1); }
    };
    rec(0);
    if (best) return best;
  }
  return upper.map(pc => 60 + pc).sort((a, b) => a - b);
}
// Bass notes live in A1..A2 and take the nearest octave to the previous bass.
const tonicBass = key => key <= 8 ? 36 + key : 24 + key;
function bassNote(pc, prev) {
  let best = null;
  for (let m = 33; m <= 45; m++) if (mod12(m) === pc && (best == null || Math.abs(m - prev) < Math.abs(best - prev))) best = m;
  return best;
}

// --------------------------------------------------------- inner movement ----
// Suspension: one upper note of the previous chord is held over the change,
// then steps down into the new chord (4-3, 9-8, or ♭6-5 on a minor chord).
function findSuspension(prev, cur, ch, beats) {
  const pcs = new Set(ch.iv.map(i => mod12(ch.root + i)));
  const top = Math.max(...prev), third = mod12(ch.root + (ch.q === "min" ? 3 : 4));
  let best = null;
  for (const p of prev) {
    if (pcs.has(mod12(p)) || cur.includes(p)) continue;
    if (!(ch.q === "min" ? [5, 2, 8] : [5, 2]).includes(mod12(p - ch.root))) continue;   // 4-3, 9-8, ♭6-5; never a held sixth
    if (ch.bass != null && mod12(p - ch.bass) === 1) continue;
    for (const n of cur) {
      const d = p - n;
      if (d !== 1 && d !== 2) continue;
      if (cur.some(x => x !== n && (Math.abs(x - p) === 1 || Math.abs(x - p) === 13))) continue;
      const score = 1 + (mod12(n) === third ? 1.2 : 0) + (p === top ? .6 : 0) + (d === 1 ? .3 : 0);
      if (!best || score > best.score) best = { p, n, score };
    }
  }
  if (!best) return null;
  return { beats, notes: cur.map(x => x === best.n ? best.p : x).sort((a, b) => a - b), held: best.p, to: best.n };
}
// Tail: in the last part of a chord, one voice that is about to move a third
// passes through the scale note in between; the other voices hold.
function findTail(cur, next, ch, scale, beats) {
  if (cur.length !== next.length) return null;
  const pcs = new Set(ch.iv.map(i => mod12(ch.root + i)));
  const okIv = ch.q === "min" ? [2, 10, 5] : [2, 11];
  let best = null;
  for (let k = 0; k < cur.length; k++) {
    const X = cur[k], Y = next[k], d = Y - X;
    if (Math.abs(d) !== 3 && Math.abs(d) !== 4) continue;
    for (const P of d > 0 ? [X + 1, X + 2] : [X - 1, X - 2]) {
      if (P === Y || Math.abs(Y - P) > 2 || !scale.includes(mod12(P)) || pcs.has(mod12(P))) continue;
      if (!okIv.includes(mod12(P - ch.root))) continue;
      if (ch.bass != null && mod12(P - ch.bass) === 1) continue;
      if (cur.some((x, j) => j !== k && (Math.abs(x - P) <= 1 || Math.abs(x - P) === 13))) continue;
      const score = 1 + (k === cur.length - 1 ? 1 : 0) + (d < 0 ? .5 : 0);
      if (!best || score > best.score) best = { X, P, score };
    }
  }
  if (!best) return null;
  return { beats, notes: cur.map(x => x === best.X ? best.P : x).sort((a, b) => a - b), from: best.X, to: best.P };
}

// ---------------------------------------------------------------- lead-in ----
// A short chord that takes the end of the bar before its target.
function makeLeadIn(proj, target, prev, rand) {
  const t = degOf(proj, target), minor = target.q === "min";
  const opts = [
    { w: 1.0, deg: t - 1, q: "maj", s: "", iv: [0,4,7], sharp: true },                                // a half step below
    { w: 0.9, deg: t + 5, q: minor ? "min" : "maj", s: minor ? "m" : "", iv: minor ? [0,3,7] : [0,4,7] },   // its IV (plagal)
    { w: 0.7, deg: t + 7, q: "maj", s: "sus4", iv: [0,5,7] },                                         // its V, suspended
  ].filter(o => mod12(proj.key + o.deg) !== prev.root);
  const o = pickWeighted(opts, opts.map(x => x.w), rand);
  const ch = { root: mod12(proj.key + o.deg), q: o.q, ext: o.s, iv: o.iv, kind: "approach", sharp: !!o.sharp, notes: [], bass: null };
  ch.beats = Math.max(1, Math.min(proj.leadBeats, proj.bpb - 1));
  return ch;
}

// ------------------------------------------------------------------- roll ----
const emptySlot = beats => ({ locked: false, chord: null, beats, sus: null, tail: null, leadIn: null });
const sectionBars = (proj, sec) => sec.slots.reduce((a, s) => a + s.beats, 0) / proj.bpb;
const secLength = sec => (sec.length | 0) === 4 ? 4 : 8;
function slotBassType(proj, sec, i) {
  const ch = sec.slots[i].chord, b = mod12(ch.bass - proj.key);
  if (b !== degOf(proj, ch)) return b === 0 ? "T" : b === fifthDeg ? "F" : "R";
  for (const j of [i - 1, i + 1]) {
    const o = sec.slots[j] && sec.slots[j].chord;
    if (o && o.bass != null && mod12(o.bass) !== o.root && mod12(o.bass) === mod12(ch.bass)) return b === 0 ? "T" : "F";
  }
  return "R";
}
function rerollCandidate(proj, sec, i, bt, rand) {
  const n = sec.slots.length, cur = asCand(proj, sec.slots[i].chord);
  const prev = i > 0 ? asCand(proj, sec.slots[i-1].chord) : null, next = i + 1 < n ? asCand(proj, sec.slots[i+1].chord) : null;
  if ((sec.role === "home" && i === 0) || (sec.role === "return" && i === n - 1)) return null;     // these stay on the tonic
  const rel = ch => new Set([...chordPcs(ch)].map(p => mod12(p - proj.key)));
  const P = i > 0 ? rel(sec.slots[i-1].chord) : null, N = i + 1 < n ? rel(sec.slots[i+1].chord) : null;
  const joins = c => { const bd = bassDeg(c, bt); return palette(proj, c, bd !== c.deg, bd).some(e => { const S = extPcs(c, e, bd); return (!P || overlap(S, P)) && (!N || overlap(S, N)); }); };
  const cands = vocab(proj.mode).filter(c => ckey(c) !== ckey(cur) && (!prev || ckey(c) !== ckey(prev)) && (!next || ckey(c) !== ckey(next)) && (bt === "R" || pedalOk(c, bt)) && joins(c));
  if (!cands.length) return null;
  const inSec = new Set(sec.slots.filter(s => s.chord).map(s => ckey(asCand(proj, s.chord))));
  const w = cands.map(c => (inSec.has(ckey(c)) ? 2 : 1) * (1 + (prev ? commonTones(prev, c) : 1)) * (1 + (next ? commonTones(c, next) : 1)) * (c.deg === 4 ? .5 : 1)
    * (!MODES[proj.mode].minor && c.q === "min" ? .4 + 1.6 * proj.mood.wistful : 1));
  return { ...pickWeighted(cands, w, rand), kind: "diatonic" };
}

// Roll a section. `only` (Set of slot indexes) rerolls just those chords.
// Locked slots are never touched: notes, bass, suspension, tail and lead-in stay.
function rollSection(proj, sec, only, seed) {
  const rand = makeRng(seed ?? Math.floor(Math.random() * 2 ** 32));
  if (proj.officeSafe && MODES[proj.mode].minor) proj.mode = "ionian";       // with this switch on, a minor mode becomes major
  const m = proj.mood, beats = proj.bpb * Math.max(1, sec.chordBars | 0 || 1);
  let seq, types, n;
  const keptOld = i => sec.slots[i] && sec.slots[i].chord && sec.slots[i].chord.notes.length && (sec.slots[i].locked || (only && !only.has(i)));

  if (only) {
    n = sec.slots.length;
    seq = sec.slots.map(s => asCand(proj, s.chord));
    const planned = sec.plan && PLANS[sec.plan.id] && (n === 4 || n === 8) ? PLANS[sec.plan.id].types(n) : null;
    types = sec.slots.map((s, i) => planned ? planned[i] : slotBassType(proj, sec, i));
    if (sec.role === "return") types[n - 1] = "T";
    for (const i of only) {
      if (!sec.slots[i] || sec.slots[i].locked) continue;
      const c = rerollCandidate(proj, sec, i, types[i], rand);
      if (c) seq[i] = c;
    }
  } else {
    n = secLength(sec);
    const old = sec.slots, locked = [];
    for (let i = 0; i < n; i++) locked[i] = old[i] && old[i].locked && old[i].chord && old[i].chord.notes.length ? asCand(proj, old[i].chord) : null;
    const planId = choosePlan(proj, sec, rand);
    let sk = null;
    for (let tries = 0; tries < 40; tries++) {
      sk = chooseSkeleton(proj, sec, n, planId, locked, rand);
      borrowOne(proj, sec, sk.seq, sk.types, i => !!locked[i], rand);
      const bd = sk.seq.map((c, i) => locked[i] ? null : bassDeg(c, sk.types[i]));
      const giv = locked.map((L, i) => L ? new Set([...chordPcs(old[i].chord)].map(p => mod12(p - proj.key))) : null);
      const trial = colorize(proj, sk.seq, bd, giv, false, makeRng(1), sec.role === "return");
      let dull = false;                                    // never the same chord three times running
      for (let i = 2; i < n; i++) if (ckey(sk.seq[i]) === ckey(sk.seq[i-1]) && ckey(sk.seq[i]) === ckey(sk.seq[i-2])) dull = true;
      if (new Set(sk.seq.filter(c => c.kind !== "borrowed").map(ckey)).size > 4) dull = true;      // small vocabulary
      if (trial.ok && !dull) break;
    }
    seq = sk.seq; types = sk.types;
    sec.plan = { id: planId, label: PLANS[planId].label };
    sec.from = sk.from;
    sec.slots = [];
    for (let i = 0; i < n; i++) sec.slots.push(locked[i] ? old[i] : emptySlot(beats));
    if (sec.role === "return") types[n - 1] = "T";
  }
  const fixed = i => only ? !only.has(i) || sec.slots[i].locked : !!(sec.slots[i].locked && sec.slots[i].chord && sec.slots[i].chord.notes.length);

  // Bass degrees and color.
  const bdegs = seq.map((c, i) => fixed(i) ? mod12(sec.slots[i].chord.bass - proj.key) : bassDeg(c, types[i]));
  const given = seq.map((c, i) => fixed(i) ? new Set([...chordPcs(sec.slots[i].chord)].map(p => mod12(p - proj.key))) : null);
  const { exts } = colorize(proj, seq, bdegs, given, sec.role !== "return", rand, sec.role === "return");
  for (let i = 0; i < n; i++) {
    if (fixed(i)) continue;
    const c = seq[i], e = exts[i];
    sec.slots[i].chord = { root: mod12(proj.key + c.deg), q: c.q, ext: e.s, iv: e.iv.slice(), kind: c.kind || "diatonic", notes: [], bass: null };
  }
  // Lead-ins belong to the chord they lead into. None over a drone.
  for (let i = 0; i < n; i++) {
    const slot = sec.slots[i];
    if (fixed(i)) continue;
    slot.leadIn = null;
    if (i === 0 || proj.bpb < 2 || slot.beats < 2) continue;
    const before = sec.slots[i-1].chord;
    if (before.root === slot.chord.root || (bdegs[i] === bdegs[i-1])) continue;
    if (rand() < m.leadins) slot.leadIn = makeLeadIn(proj, slot.chord, before, rand);
  }
  // Voice in playing order; fixed slots keep their stored notes and bass.
  const home = homeSection(proj, sec), hl = home && home.slots[home.slots.length - 1];
  let prevNotes = sec.role !== "home" && hl && hl.chord && hl.chord.notes.length ? hl.chord.notes : null;
  let prevBass = tonicBass(proj.key);
  for (let i = 0; i < n; i++) {
    const slot = sec.slots[i], isFixed = fixed(i);
    if (slot.leadIn && !(isFixed && slot.leadIn.notes.length)) {
      slot.leadIn.notes = voice(slot.leadIn, prevNotes);
      slot.leadIn.bass = bassNote(slot.leadIn.root, prevBass);
    }
    if (slot.leadIn) { prevNotes = slot.leadIn.notes; prevBass = slot.leadIn.bass ?? prevBass; }
    if (!isFixed) {
      const pc = mod12(proj.key + bdegs[i]);
      slot.chord.bass = bdegs[i] === 0 ? tonicBass(proj.key) : bassNote(pc, prevBass);
      slot.chord.notes = voice(slot.chord, prevNotes);
    }
    prevNotes = slot.chord.notes; prevBass = slot.chord.bass ?? prevBass;
  }
  // Inner movement. Suspensions land where the tension lets go; slot 0 looks
  // back at the last slot, for when the section loops.
  const scaleOf = ch => (parentScale(proj.mode, asCand(proj, ch)) || MODES[proj.mode].steps).map(s => mod12(s + proj.key));
  const ten = i => { const ch = sec.slots[i].chord; return tension(asCand(proj, ch), mod12(ch.bass - proj.key)); };
  const touch = i => !fixed(i) || (only && !sec.slots[i].locked && (only.has((i + n - 1) % n) || only.has((i + 1) % n)));
  // No more than about half the chords get one, and the same suspension is not heard more than twice.
  const order = [...Array(n).keys()].map(i => [rand(), i]).sort((a, b) => a[0] - b[0]).map(x => x[1]);
  const susKey = s => mod12(s.held) + ">" + mod12(s.to), seen = {};
  let susCount = 0;
  const cap = Math.ceil(n * (.15 + .35 * m.inner));
  for (let i = 0; i < n; i++) if (!touch(i) && sec.slots[i].sus) { susCount++; seen[susKey(sec.slots[i].sus)] = (seen[susKey(sec.slots[i].sus)] || 0) + 1; }
  for (const i of order) {
    const slot = sec.slots[i];
    if (!touch(i)) continue;
    slot.sus = null;
    if (slot.leadIn || n < 2 || m.inner <= 0 || susCount >= cap) continue;
    const pi = (i + n - 1) % n, prev = sec.slots[pi].chord;
    if (prev.root === slot.chord.root) continue;
    const release = ten(i) <= ten(pi) - .15, last = sec.role === "return" && i === n - 1;
    const p = last ? .4 + .6 * m.inner : release ? Math.min(.9, m.inner * 1.2) : m.inner * .2;
    if (rand() >= p) continue;
    const s = findSuspension(prev.notes, slot.chord.notes, slot.chord, Math.min(Math.max(1, Math.floor(slot.beats / 2)), proj.bpb));
    if (!s || (seen[susKey(s)] || 0) >= 2) continue;
    slot.sus = s; susCount++; seen[susKey(s)] = (seen[susKey(s)] || 0) + 1;
  }
  for (let i = 0; i < n; i++) {
    const slot = sec.slots[i];
    if (!touch(i)) continue;
    slot.tail = null;
    if (m.inner <= 0 || n < 2 || slot.beats < 2 || (i === n - 1 && sec.role === "return")) continue;
    const nx = sec.slots[(i + 1) % n];
    if (nx.sus || nx.leadIn || (slot.sus && slot.beats < 2 * proj.bpb)) continue;
    if (rand() < m.inner * .6) slot.tail = findTail(slot.chord.notes, nx.chord.notes, slot.chord, scaleOf(slot.chord), Math.max(1, Math.floor(slot.beats / (slot.sus ? 4 : 2))));
  }
  sec.bars = sectionBars(proj, sec);
  return sec;
}

// ----------------------------------------------------------------- events ----
// Events for one section played for `bars` bars from `startBeat` (loops the
// section if the block is longer). The bass is merged across chords when it
// does not change, so a drone is one held note.
function blockEvents(proj, sec, bars, startBeat, tag) {
  const ev = [], bassEv = [], total = bars * proj.bpb, n = sec.slots.length;
  if (!n) return ev;
  const addBass = (t, len, note) => {
    if (note == null || len <= 0) return;
    const last = bassEv[bassEv.length - 1];
    if (last && last.notes[0] === note && Math.abs(last.t + last.len - t) < 1e-9) last.len += len;
    else bassEv.push({ t, len, notes: [note], bass: true });
  };
  let t = 0, i = 0;
  while (t < total - 1e-9) {
    const idx = i % n, slot = sec.slots[idx];
    if (!slot.chord) break;
    const len = Math.min(slot.beats, total - t);
    const nextIdx = (i + 1) % n, more = t + slot.beats < total;
    const lead = more && nextIdx !== 0 ? sec.slots[nextIdx].leadIn : null;
    const lb = lead ? Math.min(lead.beats || 1, len - 1) : 0;
    const sb = slot.sus && i > 0 ? Math.max(0, Math.min(slot.sus.beats, len - lb - 1)) : 0;
    const tb = slot.tail && more && !lb ? Math.max(0, Math.min(slot.tail.beats, len - sb - 1)) : 0;
    const T = startBeat + t, base = { sec: sec.id, idx, tag };
    if (sb > 0) ev.push({ ...base, t: T, len: sb, notes: slot.sus.notes, sus: true });
    ev.push({ ...base, t: T + sb, len: len - lb - sb - tb, notes: slot.chord.notes });
    if (tb > 0) ev.push({ ...base, t: T + len - tb, len: tb, notes: slot.tail.notes, tail: true });
    addBass(T, len - lb, slot.chord.bass);
    if (lb > 0) { ev.push({ ...base, t: T + len - lb, len: lb, notes: lead.notes, lead: true }); addBass(T + len - lb, lb, lead.bass); }
    t += slot.beats; i++;
  }
  return ev.concat(bassEv);
}
// Turns chord events into single notes and joins a note that continues at the
// same pitch into the next chord, within one block, so held notes are not struck again.
function tieEvents(events) {
  const out = [], open = new Map();
  const sorted = events.filter(e => !e.bass).sort((a, b) => a.t - b.t);
  for (const e of sorted) for (const note of e.notes) {
    const k = (e.tag ?? "") + ":" + note, o = open.get(k);
    if (o && Math.abs(o.t + o.len - e.t) < 1e-9) o.len += e.len;
    else { const x = { t: e.t, len: e.len, notes: [note], tag: e.tag }; open.set(k, x); out.push(x); }
  }
  return out.concat(events.filter(e => e.bass));
}

// ------------------------------------------------------------- MIDI file ----
function vlq(n) { const out = [n & 0x7f]; while ((n >>= 7)) out.unshift((n & 0x7f) | 0x80); return out; }
function track(events, name) {
  events.sort((a, b) => a.tick - b.tick || a.order - b.order);
  const bytes = [], nm = Array.from(new TextEncoder().encode(name));
  bytes.push(0, 0xff, 0x03, ...vlq(nm.length), ...nm);
  let last = 0;
  for (const e of events) { bytes.push(...vlq(e.tick - last), ...e.data); last = e.tick; }
  bytes.push(0, 0xff, 0x2f, 0);
  const len = bytes.length;
  return [0x4d,0x54,0x72,0x6b, (len>>>24)&255, (len>>>16)&255, (len>>>8)&255, len&255, ...bytes];
}
function midiFile(proj, events, bassSeparate) {
  const tempo = Math.round(60000000 / proj.tempo);
  const meta = [
    { tick: 0, order: 0, data: [0xff, 0x51, 3, (tempo>>16)&255, (tempo>>8)&255, tempo&255] },
    { tick: 0, order: 0, data: [0xff, 0x58, 4, proj.bpb, 2, 24, 8] },
  ];
  const chords = [], bass = [];
  for (const e of events) {
    const on = Math.round(e.t * PPQ), off = Math.round((e.t + e.len) * PPQ) - 1;
    const ch = e.bass && bassSeparate ? 1 : 0, dst = e.bass && bassSeparate ? bass : chords, vel = e.bass ? 80 : 72;
    for (const n of e.notes) dst.push({ tick: on, order: 1, data: [0x90 | ch, n, vel] }, { tick: off, order: 0, data: [0x80 | ch, n, 0] });
  }
  const tracks = [track(meta, "Tempo"), track(chords, "Chords")];
  if (bassSeparate) tracks.push(track(bass, "Bass"));
  const head = [0x4d,0x54,0x68,0x64, 0,0,0,6, 0,1, 0,tracks.length, (PPQ>>8)&255, PPQ&255];
  return new Uint8Array([...head, ...tracks.flat()]);
}
const playable = (proj, events) => proj.tie ? tieEvents(events) : events;

// ---------------------------------------------------------------- project ----
const uid = () => Math.random().toString(36).slice(2, 9);
const DEFAULT_MOOD = { wistful: 0.35, moving: 0.5, color: 0.5, inner: 0.5, surprise: 0.2, leadins: 0 };
const ROLE_NAMES = { home: "Verse", lift: "Chorus", away: "Middle eight", turn: "Turnaround", return: "Coda" };
function newSection(role) {
  return { id: uid(), name: ROLE_NAMES[role], role, chordBars: 1, length: role === "turn" ? 4 : 8, planPref: "auto", plan: null, from: null, slots: [] };
}
function newProject() {
  const p = { schema: SCHEMA, engine: ENGINE_VERSION, key: 0, mode: "ionian", officeSafe: true, jazz: false, tie: true, tempo: 60, bpb: 4,
    mood: { ...DEFAULT_MOOD }, leadBeats: 1, bassTrack: true, sections: [], arrangement: [], feedback: [] };
  p.sections = ["home", "lift", "away", "return"].map(newSection);
  const [A, B, C, D] = p.sections.map(s => s.id);
  p.arrangement = [[A,16],[B,8],[A,8],[C,8],[B,8],[A,8],[D,8]].map(([sectionId, bars]) => ({ id: uid(), sectionId, bars }));
  return p;
}
function rollAll(proj) {
  [...proj.sections].sort((a, b) => ROLES.indexOf(a.role) - ROLES.indexOf(b.role)).forEach(s => rollSection(proj, s));
}
// Older files are converted in place. Chords, notes, bass notes and beats are
// never changed here; only missing settings are filled in.
function migrate(p) {
  if (!p || typeof p !== "object" || !Array.isArray(p.sections)) throw new Error("This file is not a Chord Engine project.");
  if ((p.schema | 0) > SCHEMA) throw new Error(`This project was saved by a newer Chord Engine (format ${p.schema}).`);
  if ((p.schema | 0) < 2) {
    // v0 (format 1): one chord per bar, no drone or suspensions.
    p.sections.forEach(s => { s.chordBars = 1; s.reps = 1; s.pedal = false; s.loop = null;
      s.slots.forEach(sl => { sl.beats = p.bpb; sl.sus = null; }); });
    p.mood = p.mood || {}; p.mood.susp = 0; p.jazz = true; p.feedback = []; p.schema = 2;
  }
  if ((p.schema | 0) < 3) {
    // v1 (format 2): loops with a per-section pedal switch. Old files play as
    // they did: chords are struck again at every change (no ties).
    const old = p.mood || {};
    p.mood = { ...DEFAULT_MOOD, ...old, inner: old.susp ?? DEFAULT_MOOD.inner };
    p.tie = false;
    p.sections.forEach(s => {
      s.length = s.slots.length <= 4 ? 4 : 8; s.planPref = "auto"; s.plan = null; s.from = null;
      s.chordBars = s.chordBars || 1;
      s.slots.forEach(sl => { if (sl.tail === undefined) sl.tail = null; if (sl.sus === undefined) sl.sus = null; if (sl.beats == null) sl.beats = p.bpb; });
    });
    p.feedback = p.feedback || []; p.schema = 3;
  }
  return p;
}

// ------------------------------------------------------------------ chart ----
// "C [F>E]" = F is held over from the chord before, then falls to E.
// "C [~B]"  = one voice passes through B on its way to the next chord.
function slotText(proj, sec, i) {
  const sl = sec.slots[i], nx = sec.slots[i + 1];
  if (!sl.chord) return "–";
  let s = chordName(sl.chord, proj.key);
  if (sl.sus) s += ` [${pcName(sl.sus.held, proj.key)}>${pcName(sl.sus.to, proj.key)}]`;
  if (sl.tail) s += ` [~${pcName(sl.tail.to, proj.key)}]`;
  const b = sl.beats / proj.bpb;
  if (b !== 1) s += ` (${b})`;
  if (nx && nx.leadIn) s += ` ‹${chordName(nx.leadIn, proj.key)}›`;
  return s;
}
function sectionChart(proj, sec) {
  const chords = sec.slots.map((_, i) => slotText(proj, sec, i));
  const bass = sec.slots.map(sl => sl.chord && sl.chord.bass != null ? pcName(sl.chord.bass, proj.key) : "–");
  const w = chords.map((c, i) => Math.max(c.length, bass[i].length));
  const head = `${sec.name}${sec.plan ? " · " + sec.plan.label : ""}${sec.from && sec.role !== "home" ? " · from " + sec.from : ""}`;
  return `${head}\n  | ${chords.map((c, i) => c.padEnd(w[i])).join(" | ")} |\n  | ${bass.map((c, i) => c.padEnd(w[i])).join(" | ")} |  bass`;
}
function chartText(proj) {
  const arr = proj.arrangement.map(b => { const s = proj.sections.find(x => x.id === b.sectionId); return `${s ? s.name : "?"} ×${b.bars}`; }).join(", ");
  return `Key ${keyName(proj.key)} ${MODES[proj.mode].name} · ${proj.tempo} BPM · ${proj.bpb}/4\n\n` + proj.sections.map(s => sectionChart(proj, s)).join("\n\n") + `\n\nArrangement (bars): ${arr}`;
}

if (typeof module !== "undefined") module.exports = { ENGINE_VERSION, SCHEMA, PPQ, MODES, MODE_ORDER, CELLS, PLANS, PLAN_ORDER, ROLES, EXTS, TOK,
  newProject, newSection, rollSection, rollAll, blockEvents, tieEvents, playable, midiFile, migrate, chordName, romanName, keyName, pcName,
  isDiatonic, parentScale, degOf, asCand, chordPcs, sharesTone, pedalOk, allowedPlans, sectionBars, sectionChart, chartText, tonicBass, makeRng };
