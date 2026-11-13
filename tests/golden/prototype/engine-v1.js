"use strict";
// ---------------------------------------------------------------- engine ----
// v1: sections are built from short known loops (2-4 chords) that repeat,
// sections are related to each other through shared chords, extensions come
// from a warm palette, optional tonic pedal, and suspensions (a note of the
// previous chord held over, then stepping down). Everything rolled is stored
// as explicit notes and beats, never as a seed.
const ENGINE_VERSION = "1.0.0";
const SCHEMA = 2;
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
const EXTS = {
  maj: [ {s:"",iv:[0,4,7],t:"plain"}, {s:"add9",iv:[0,4,7,14],t:"add9"}, {s:"sus2",iv:[0,2,7],t:"sus2"},
         {s:"maj7",iv:[0,4,7,11],t:"maj7"}, {s:"sus4",iv:[0,5,7],t:"sus4"},
         {s:"6",iv:[0,4,7,9],t:"jazz"}, {s:"6/9",iv:[0,4,7,9,14],t:"jazz"}, {s:"maj9",iv:[0,4,7,11,14],t:"jazz"},
         {s:"maj7♯11",iv:[0,4,7,11,18],t:"jazz"}, {s:"9sus4",iv:[0,5,7,10,14],t:"jazz"} ],
  min: [ {s:"m",iv:[0,3,7],t:"plain"}, {s:"m7",iv:[0,3,7,10],t:"maj7"}, {s:"m(add9)",iv:[0,3,7,14],t:"add9"},
         {s:"m9",iv:[0,3,7,10,14],t:"jazz"}, {s:"m11",iv:[0,3,7,10,14,17],t:"jazz"}, {s:"m6",iv:[0,3,7,9],t:"jazz"} ],
  dim: [ {s:"°",iv:[0,3,6],t:"plain"} ],
};

// Loop library: short progressions that have carried a great deal of pop,
// new age and ambient music. Chords are semitones above the tonic + quality.
// Loops that do not fit the chosen mode are filtered out automatically.
const LOOPS = [
  { id: "I-IV",       c: ["0M","5M"],           w: 1.6, f: "float" },
  { id: "I-vi",       c: ["0M","9m"],           w: 1.2, f: "float" },
  { id: "I-ii",       c: ["0M","2m"],           w: 0.9, f: "float" },
  { id: "I-V",        c: ["0M","7M"],           w: 0.9, f: "both"  },
  { id: "I-vi-IV",    c: ["0M","9m","5M"],      w: 1.5, f: "float" },
  { id: "I-IV-vi-IV", c: ["0M","5M","9m","5M"], w: 1.2, f: "float" },
  { id: "I-iii-IV",   c: ["0M","4m","5M"],      w: 0.8, f: "float" },
  { id: "I-ii-IV",    c: ["0M","2m","5M"],      w: 0.9, f: "float" },
  { id: "I-IV-I-V",   c: ["0M","5M","0M","7M"], w: 0.9, f: "drive" },
  { id: "I-V-vi-IV",  c: ["0M","7M","9m","5M"], w: 1.0, f: "drive" },
  { id: "I-vi-ii-V",  c: ["0M","9m","2m","7M"], w: 0.5, f: "drive" },
  { id: "I-IV-V-IV",  c: ["0M","5M","7M","5M"], w: 0.7, f: "drive" },
  { id: "I-II",       c: ["0M","2M"],           w: 1.6, f: "float" },  // Lydian
  { id: "I-II-vi",    c: ["0M","2M","9m"],      w: 1.0, f: "float" },
  { id: "I-V-II",     c: ["0M","7M","2M"],      w: 0.8, f: "float" },
  { id: "I-bVII",     c: ["0M","10M"],          w: 1.5, f: "both"  },  // Mixolydian
  { id: "I-bVII-IV",  c: ["0M","10M","5M"],     w: 1.4, f: "both"  },
  { id: "I-v-IV",     c: ["0M","7m","5M"],      w: 0.8, f: "float" },
  { id: "i-IV",       c: ["0m","5M"],           w: 1.4, f: "float" },  // Dorian
  { id: "i-bVII-IV",  c: ["0m","10M","5M"],     w: 1.2, f: "both"  },
  { id: "i-bIII-IV",  c: ["0m","3M","5M"],      w: 0.8, f: "float" },
  { id: "i-bVI-bVII", c: ["0m","8M","10M"],     w: 1.3, f: "both"  },  // Minor
  { id: "i-bVI-bIII-bVII", c: ["0m","8M","3M","10M"], w: 1.1, f: "both" },
  { id: "i-iv",       c: ["0m","5m"],           w: 0.9, f: "float" },
];
const parse = s => ({ deg: parseInt(s, 10), q: s.endsWith("m") ? "min" : "maj" });
const ckey = c => `${c.deg}${c.q}`;
const ROMAN = ["I","♭II","II","♭III","III","IV","♯IV","V","♭VI","VI","♭VII","VII"];
function loopLabel(chords) {
  return chords.map(c => { const n = ROMAN[c.deg]; return c.q === "maj" ? n : n.toLowerCase(); }).join("–");
}

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
function commonTones(a, b) {
  const A = new Set(TRIAD[a.q].map(i => mod12(a.deg + i)));
  return TRIAD[b.q].filter(i => A.has(mod12(b.deg + i))).length;
}
function loopsForMode(mode) {
  return LOOPS.map(l => ({ ...l, chords: l.c.map(parse) })).filter(l => l.chords.every(c => isDiatonic(mode, c)));
}

// One substitution per section, chosen by the Surprise slider.
const RELATIVE = { "0maj": [9,"min"], "5maj": [2,"min"], "9min": [0,"maj"], "2min": [5,"maj"], "7maj": [4,"min"] };
const BORROWED = { "5maj": [5,"min"], "7maj": [10,"maj"], "9min": [8,"maj"], "2min": [10,"maj"] };

function pickExtension(proj, c, bias, rand) {
  const m = proj.mood, r = m.color, f = 1 - m.drive;
  const scale = parentScale(proj.mode, c);
  const allowed = EXTS[c.q].filter(e => (proj.jazz || e.t !== "jazz") && (!scale || e.iv.every(i => scale.includes(mod12(c.deg + i)))));
  const weights = allowed.map(e => {
    const w = { plain: 0.3 + 1.6 * (1 - r), add9: 1.2 * r, sus2: 0.6 * r * (0.4 + f), maj7: 0.6 * r,
      sus4: c.deg === 7 ? 0.5 * r : 0.1 * r, jazz: 0.35 * r }[e.t];
    return e.t === bias ? w * 2.5 : w;
  });
  return pickWeighted(allowed, weights, rand);
}

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
  return n + s;
}
function makeChord(proj, c, ext, kind) {
  return { root: mod12(proj.key + c.deg), q: c.q, ext: ext.s, iv: ext.iv.slice(), kind, notes: [], bass: null };
}
const degOf = (proj, ch) => mod12(ch.root - proj.key);
const asCand = (proj, ch) => ({ deg: degOf(proj, ch), q: ch.q in TRIAD ? ch.q : "maj" });

function makeLeadIn(proj, target, rand) {
  const j = proj.mood.jagged, t = degOf(proj, target);
  const opts = [
    { w: 0.1 + 0.9 * j, deg: t - 1, q: "dim", s: "°7",   iv: [0,3,6,9],  sharp: true },
    { w: 0.6,           deg: t - 1, q: "maj", s: "",     iv: [0,4,7],    sharp: true },
    { w: 0.3 + 0.5 * j, deg: t + 1, q: "maj", s: "maj7", iv: [0,4,7,11] },
    { w: 1.0,           deg: t + 7, q: "maj", s: "7",    iv: [0,4,7,10] },
  ];
  const o = pickWeighted(opts, opts.map(x => x.w), rand);
  const ch = { root: mod12(proj.key + o.deg), q: o.q, ext: o.s, iv: o.iv, kind: "approach", sharp: !!o.sharp, notes: [], bass: null };
  ch.beats = Math.min(proj.leadBeats, proj.bpb - 1);
  return ch;
}

// Voicing: upper voices in D3..G5, moving as little as possible, top voice
// held where it can be. Bass = chord root, or the tonic when pedal is on.
function voice(ch, prevNotes, jagged, pedalPc) {
  const pcs = [];
  ch.iv.forEach(i => { const p = mod12(ch.root + i); if (!pcs.includes(p)) pcs.push(p); });
  let upper = pcs.slice();
  if (upper.length >= 6) upper = upper.filter(p => p !== ch.root && p !== mod12(ch.root + 7));
  else if (upper.length === 5) upper = upper.filter(p => p !== ch.root);
  if (upper.length === 3) upper.push(ch.root);
  const LO = 50, HI = 79;
  const options = upper.map(pc => { const a = []; for (let m = LO; m <= HI; m++) if (mod12(m) === pc) a.push(m); return a; });
  const bass = 36 + (pedalPc != null ? pedalPc : mod12(ch.root));
  for (const minGap of jagged < 0.5 ? [2, 1] : [1]) {
    let best = null, bestCost = Infinity;
    const combo = new Array(upper.length);
    const rec = k => {
      if (k === upper.length) {
        const s = combo.slice().sort((a, b) => a - b);
        for (let i = 1; i < s.length; i++) { const g = s[i] - s[i-1]; if (g < minGap || (s[i] < 57 && g < 5)) return; }
        if (s[s.length-1] - s[0] > (jagged < 0.5 ? 19 : 26)) return;
        const mean = s.reduce((a, b) => a + b, 0) / s.length;
        let cost;
        if (prevNotes && prevNotes.length) {
          const p = prevNotes, near = (x, arr) => Math.min(...arr.map(y => Math.abs(x - y)));
          cost = (s.reduce((a, x) => a + near(x, p), 0) + p.reduce((a, y) => a + near(y, s), 0)) / 2;
          const leap = Math.abs(s[s.length-1] - p[p.length-1]);
          cost += 0.9 * leap + (leap > 4 ? 2 : 0) + 0.08 * Math.abs(mean - 64);
        } else {
          cost = Math.abs(mean - 63);
          for (let i = 1; i < s.length; i++) if (s[i] - s[i-1] < 3) cost += 0.5;
        }
        if (cost < bestCost) { bestCost = cost; best = s; }
        return;
      }
      for (const m of options[k]) { combo[k] = m; rec(k + 1); }
    };
    rec(0);
    if (best) return { notes: best, bass };
  }
  return { notes: upper.map(pc => 60 + pc).sort((a, b) => a - b), bass };
}

// A suspension: one upper note of the previous chord is held over the change,
// then steps down (1 or 2 semitones) into the new chord. Top voice preferred.
function findSuspension(prev, cur, ch, beats) {
  const pcs = new Set(ch.iv.map(i => mod12(ch.root + i)));
  const top = Math.max(...prev);
  let best = null;
  for (const p of prev) {
    if (pcs.has(mod12(p))) continue;
    for (const n of cur) {
      const d = p - n;
      if ((d === 1 || d === 2) && !cur.includes(p)) {
        const score = (p === top ? 2 : 1) + (d === 1 ? 0.2 : 0);
        if (!best || score > best.score) best = { p, n, score };
      }
    }
  }
  if (!best) return null;
  return { beats, notes: cur.map(x => x === best.n ? best.p : x).sort((a, b) => a - b), held: best.p, to: best.n };
}

const emptySlot = beats => ({ locked: false, chord: null, beats, sus: null, leadIn: null });
const sectionBars = (proj, sec) => sec.slots.reduce((a, s) => a + s.beats, 0) / proj.bpb;
function homeChords(proj, sec) {
  const h = proj.sections.find(s => s !== sec && s.role === "home" && s.slots.some(x => x.chord));
  return h ? new Set(h.slots.filter(x => x.chord).map(x => ckey(asCand(proj, x.chord)))) : null;
}

function chooseLoop(proj, sec, rand) {
  const m = proj.mood, ref = homeChords(proj, sec);
  const homeLoop = (proj.sections.find(s => s !== sec && s.role === "home") || {}).loop;
  const loops = loopsForMode(proj.mode);
  const weights = loops.map(l => {
    let w = l.w * (l.f === "drive" ? 0.2 + 1.6 * m.drive : l.f === "float" ? 1.3 - 0.8 * m.drive : 1);
    if (ref && sec.role !== "home") {
      const shared = new Set(l.chords.map(ckey).filter(k => ref.has(k))).size;
      w *= 0.15 + shared;
      if (homeLoop && homeLoop.id === l.id) w *= sec.role === "away" ? 0.15 : 0.7;
    }
    if (sec.role === "away" && l.chords.length < 3) w *= 0.6;
    return w;
  });
  const loop = pickWeighted(loops, weights, rand);
  // Rotation: where in the loop the section starts.
  const L = loop.chords.length;
  const rots = [...Array(L).keys()];
  const rw = rots.map(r => {
    const first = loop.chords[r], last = loop.chords[(r + L - 1) % L];
    if (sec.role === "home") return first.deg === 0 ? 1 : 0;
    if (sec.role === "lift") return first.deg === 0 ? 0.2 : ({ 5: 2.5, 9: 2, 2: 1.2, 7: 1, 10: 1.5 }[first.deg] || 1);
    if (sec.role === "away") return first.deg === 0 ? 0.05 : 1;
    if (sec.role === "return") return last.deg === 0 ? (loop.chords[(r + L - 2) % L].deg === 5 ? 3 : 1) : 0;
    return 1;
  });
  const r = pickWeighted(rots, rw, rand);
  const seq = rots.map(k => loop.chords[(r + k) % L]).map(c => ({ ...c, kind: "diatonic" }));
  // One substitution, never on the first chord or on the resolving last chord of a coda.
  const subP = m.surprise * (sec.role === "away" ? 0.9 : 0.5);
  if (rand() < subP && L > 1) {
    const pos = 1 + Math.floor(rand() * (L - 1));
    if (!(sec.role === "return" && pos === L - 1)) {
      const c = seq[pos], k = ckey(c);
      const useBorrowed = !MODES[proj.mode].minor && rand() < m.surprise * 0.6 && BORROWED[k];
      const sub = useBorrowed ? BORROWED[k] : RELATIVE[k];
      if (sub) {
        const cand = { deg: sub[0], q: sub[1] };
        const ok = useBorrowed ? !isDiatonic(proj.mode, cand) && parentScale(proj.mode, cand) : isDiatonic(proj.mode, cand);
        const nb = [seq[pos - 1], seq[(pos + 1) % L]];
        if (ok && nb.every(x => ckey(x) !== ckey(cand))) seq[pos] = { ...cand, kind: useBorrowed ? "borrowed" : "diatonic" };
      }
    }
  }
  return { loop: { id: loop.id, label: loopLabel(seq) }, seq };
}

function rerollOne(proj, sec, i, rand) {
  const n = sec.slots.length, cur = asCand(proj, sec.slots[i].chord);
  const prev = i > 0 ? sec.slots[i-1].chord : null, next = i + 1 < n ? sec.slots[i+1].chord : null;
  const pool = [0, 2, 4, 5, 7, 9, 10].flatMap(d => ["maj", "min"].map(q => ({ deg: d, q }))).filter(c => isDiatonic(proj.mode, c));
  const cands = pool.filter(c => ckey(c) !== ckey(cur) && (!prev || ckey(c) !== ckey(asCand(proj, prev))) && (!next || ckey(c) !== ckey(asCand(proj, next))));
  if (!cands.length) return null;
  const inLoop = new Set(sec.slots.filter(s => s.chord).map(s => ckey(asCand(proj, s.chord))));
  const w = cands.map(c => (inLoop.has(ckey(c)) ? 2 : 1) * (1 + (prev ? commonTones(asCand(proj, prev), c) : 1)) * (1 + (next ? commonTones(c, asCand(proj, next)) : 1)) * (c.deg === 4 ? 0.5 : 1));
  return { ...pickWeighted(cands, w, rand), kind: "diatonic" };
}

// Roll a section. `only` (Set of slot indexes) rerolls just those chords.
// Locked slots are never touched, including their notes, suspension and lead-in.
function rollSection(proj, sec, only, seed) {
  const rand = makeRng(seed ?? Math.floor(Math.random() * 2 ** 32));
  const m = proj.mood, beats = proj.bpb * (sec.chordBars || 2);
  const fixed = i => sec.slots[i] && sec.slots[i].chord && (sec.slots[i].locked || (only && !only.has(i)));
  const bias = pickWeighted(["add9", "maj7", "sus2", "plain"], [1.2, 0.8, 0.5 * (1.4 - m.drive), 0.6], rand);
  const extFor = {};
  const chordFor = c => {
    const k = ckey(c) + c.kind;
    if (!extFor[k]) extFor[k] = pickExtension(proj, c, bias, rand);
    return makeChord(proj, c, extFor[k], c.kind);
  };
  if (only) {
    for (const i of only) if (!sec.slots[i].locked) { const c = rerollOne(proj, sec, i, rand); if (c) sec.slots[i].chord = chordFor(c); }
  } else {
    const { loop, seq } = chooseLoop(proj, sec, rand);
    sec.loop = loop;
    const reps = Math.max(1, sec.reps | 0), n = seq.length * reps;
    const old = sec.slots;
    sec.slots = [];
    for (let i = 0; i < n; i++) {
      if (old[i] && old[i].locked && old[i].chord) { sec.slots.push(old[i]); continue; }
      const s = emptySlot(beats); s.chord = chordFor(seq[i % seq.length]); sec.slots.push(s);
    }
  }
  const n = sec.slots.length;
  const pedalPc = sec.pedal ? proj.key : null;
  // Lead-ins (rare by default) belong to the chord they lead into.
  for (let i = 0; i < n; i++) {
    const slot = sec.slots[i];
    if (fixed(i)) continue;
    slot.leadIn = null;
    if (i === 0 || proj.bpb < 2) continue;
    const before = sec.slots[i-1].chord;
    if (before.root !== slot.chord.root && rand() < m.leadins) slot.leadIn = makeLeadIn(proj, slot.chord, rand);
  }
  // Voice in playing order; fixed slots keep their stored notes.
  let prevNotes = null;
  for (let i = 0; i < n; i++) {
    const slot = sec.slots[i], isFixed = fixed(i) && slot.chord.notes.length;
    if (slot.leadIn && !(isFixed && slot.leadIn.notes.length)) Object.assign(slot.leadIn, voice(slot.leadIn, prevNotes, m.jagged, pedalPc));
    if (slot.leadIn) prevNotes = slot.leadIn.notes;
    if (!isFixed) Object.assign(slot.chord, voice(slot.chord, prevNotes, m.jagged, pedalPc));
    prevNotes = slot.chord.notes;
  }
  // Suspensions over the change (slot 0 uses the last slot, for when the section loops).
  for (let i = 0; i < n; i++) {
    const slot = sec.slots[i];
    if (fixed(i)) continue;
    slot.sus = null;
    if (slot.leadIn || n < 2) continue;
    const prev = sec.slots[(i + n - 1) % n].chord;
    if (prev.root === slot.chord.root) continue;
    if (rand() < m.susp) slot.sus = findSuspension(prev.notes, slot.chord.notes, slot.chord, Math.min(Math.max(1, Math.floor(slot.beats / 2)), proj.bpb));
  }
  sec.bars = sectionBars(proj, sec);
  return sec;
}

// Events for one section played for `bars` bars from `startBeat` (loops the
// section if the block is longer). Bass is merged across chords when it does
// not change, so a pedal is one held note.
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
    const nextIdx = (i + 1) % n;
    const lead = t + slot.beats < total && nextIdx !== 0 ? sec.slots[nextIdx].leadIn : null;
    const lb = lead ? Math.min(lead.beats || 1, len - 1) : 0;
    const sb = slot.sus && i > 0 ? Math.max(0, Math.min(slot.sus.beats, len - lb - 1)) : 0;
    const T = startBeat + t, base = { sec: sec.id, idx, tag };
    if (sb > 0) ev.push({ ...base, t: T, len: sb, notes: slot.sus.notes, sus: true });
    ev.push({ ...base, t: T + sb, len: len - lb - sb, notes: slot.chord.notes });
    addBass(T, len - lb, slot.chord.bass);
    if (lb > 0) { ev.push({ ...base, t: T + len - lb, len: lb, notes: lead.notes, lead: true }); addBass(T + len - lb, lb, lead.bass); }
    t += slot.beats; i++;
  }
  return ev.concat(bassEv);
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

const uid = () => Math.random().toString(36).slice(2, 9);
function newProject() {
  const p = { schema: SCHEMA, engine: ENGINE_VERSION, key: 0, mode: "ionian", officeSafe: true, jazz: false, tempo: 60, bpb: 4,
    mood: { drive: 0.15, jagged: 0.1, surprise: 0.25, color: 0.45, susp: 0.45, leadins: 0 }, leadBeats: 1, bassTrack: true,
    sections: [], arrangement: [], feedback: [] };
  const mk = (name, role, reps, pedal) => ({ id: uid(), name, role, chordBars: 2, reps, pedal, loop: null, slots: [] });
  p.sections = [mk("Verse", "home", 2, true), mk("Chorus", "lift", 1, false), mk("Middle eight", "away", 1, false), mk("Coda", "return", 1, true)];
  const [A, B, C, D] = p.sections.map(s => s.id);
  p.arrangement = [[A,16],[B,8],[A,16],[C,8],[B,8],[A,8],[D,8]].map(([sectionId, bars]) => ({ id: uid(), sectionId, bars }));
  return p;
}
function rollAll(proj) {
  const order = ["home", "lift", "away", "return"];
  [...proj.sections].sort((a, b) => order.indexOf(a.role) - order.indexOf(b.role)).forEach(s => rollSection(proj, s));
}
function migrate(p) {
  if (!p || typeof p !== "object" || !Array.isArray(p.sections)) throw new Error("This file is not a Chord Engine project.");
  if ((p.schema | 0) > SCHEMA) throw new Error(`This project was saved by a newer Chord Engine (format ${p.schema}).`);
  if ((p.schema | 0) < 2) {
    // v0 (format 1): one chord per bar, no pedal or suspensions. Notes are kept exactly.
    p.sections.forEach(s => { s.chordBars = 1; s.reps = 1; s.pedal = false; s.loop = null;
      s.slots.forEach(sl => { sl.beats = p.bpb; sl.sus = null; }); });
    p.mood.susp = 0; p.jazz = true; p.feedback = []; p.schema = 2;
  }
  return p;
}
if (typeof module !== "undefined") module.exports = { newProject, rollSection, rollAll, blockEvents, midiFile, migrate, chordName, romanName, MODE_ORDER, MODES, isDiatonic, degOf, asCand };
