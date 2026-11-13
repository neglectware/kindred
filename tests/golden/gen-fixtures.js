"use strict";
// node tests/golden/gen-fixtures.js <out.jsonl> [projects] [seed]
// Runs the prototype engines (prototype/engine-v2.js, and prototype/engine-v1.js for migration)
// with explicit seeds and writes what it produced. tests/golden/GoldenTest.cpp
// replays the same operations in the C++ engine and compares every result.
const path = require("path"), fs = require("fs");
const E = require(path.join(__dirname, "prototype", "engine-v2.js"));
const V1 = require(path.join(__dirname, "prototype", "engine-v1.js"));
const OUT = process.argv[2], COUNT = +process.argv[3] || 3000, SEED = +process.argv[4] || 20261112;
const rnd = E.makeRng(SEED);
Math.random = rnd;                                  // ids and the v1 engine draw from Math.random
const pick = a => a[Math.floor(rnd() * a.length)];
const seed = () => Math.floor(rnd() * 2 ** 32);
const mod12 = n => ((n % 12) + 12) % 12;

// ---- canonical text and hash (the same definitions as src/engine/EngineIO.cpp) ----
const num = x => Number.isInteger(x) ? String(x) : x.toFixed(6);
const canonChord = (c, lead) => !c ? "-" : [c.root, c.q, c.ext, c.iv.join("."), c.kind || "diatonic", c.notes.join("."), c.bass == null ? "-" : c.bass, c.sharp ? 1 : 0].join(",") + (lead ? "," + num(c.beats || 0) : "");
const canonInner = (s, k) => !s ? "-" : [num(s.beats), s.notes.join("."), s[k], s.to].join(",");
function canonText(p) {
  let out = `K${p.key} M${p.mode}\n`;
  for (const s of p.sections) {
    out += `S ${s.role} ${s.plan ? s.plan.id : "-"} ${s.from || "-"} n=${s.slots.length}\n`;
    for (const sl of s.slots) out += ` L${sl.locked ? 1 : 0} b${num(sl.beats)} C[${canonChord(sl.chord, false)}] S[${canonInner(sl.sus, "held")}] T[${canonInner(sl.tail, "from")}] I[${canonChord(sl.leadIn, true)}]\n`;
  }
  return out;
}
const canonEvents = ev => ev.map(e => `${num(e.t)} ${num(e.len)} ${e.notes.join(".")} ${(e.bass ? "b" : "") + (e.sus ? "s" : "") + (e.tail ? "t" : "") + (e.lead ? "l" : "")} ${e.sec || "-"} ${e.idx ?? -1} ${e.tag ?? "-"}\n`).join("");
function fnv(bytes) {                                // FNV-1a 64 in two 32-bit halves
  let hi = 0xcbf29ce4, lo = 0x84222325;
  for (let i = 0; i < bytes.length; i++) {
    lo ^= bytes[i];
    // multiply by 0x100000001b3 = 2^40 + 0x1b3
    const a = (lo >>> 0) * 0x1b3, loNew = a >>> 0, carry = Math.floor(a / 4294967296);
    hi = (Math.imul(hi, 0x1b3) + carry + ((lo << 8) >>> 0)) >>> 0;
    lo = loNew;
  }
  return (hi >>> 0).toString(16).padStart(8, "0") + (lo >>> 0).toString(16).padStart(8, "0");
}
const hashText = s => fnv(Buffer.from(s, "utf8"));

function arrangementEvents(eng, p) {
  const ev = []; let beat = 0;
  p.arrangement.forEach(b => { const s = p.sections.find(x => x.id === b.sectionId); if (!s) return; ev.push(...eng.blockEvents(p, s, b.bars, beat, b.id)); beat += b.bars * p.bpb; });
  return ev;
}
const rollAll = (p, seeds) => { [...p.sections].sort((a, b) => E.ROLES.indexOf(a.role) - E.ROLES.indexOf(b.role)).forEach((s, i) => E.rollSection(p, s, undefined, seeds[i])); };

const out = fs.createWriteStream(OUT);
const stats = { fixtures: 0, ops: 0 };
function finish(fx, p) {
  fx.canon = canonText(p);
  const ev = arrangementEvents(E, p);
  fx.events = hashText(canonEvents(ev));
  fx.tied = hashText(canonEvents(E.tieEvents(ev)));
  const midi = E.midiFile(p, E.playable(p, ev), p.bassTrack);
  fx.midi = fnv(midi); fx.midiLen = midi.length;
  const s0 = p.sections[0];
  if (s0 && s0.slots.length && s0.slots.every(x => x.chord)) {
    const sm = E.midiFile(p, E.playable(p, E.blockEvents(p, s0, E.sectionBars(p, s0), 0)), p.bassTrack);
    fx.secMidi = fnv(sm); fx.secMidiLen = sm.length;
  }
  fx.chart = hashText(E.chartText(p));
  out.write(JSON.stringify(fx) + "\n");
  stats.fixtures++; stats.ops += fx.ops.length;
}
// Applies one operation to the JS project and records it (with the state hash after it).
function apply(fx, p, op) {
  const sec = op.sec != null ? p.sections[op.sec] : null;
  switch (op.op) {
    case "rollAll": rollAll(p, op.seeds); break;
    case "roll": E.rollSection(p, sec, undefined, op.seed); break;
    case "reroll": E.rollSection(p, sec, new Set([op.slot]), op.seed); break;
    case "lock": sec.slots[op.slot].locked = op.v; break;
    case "unlockAll": p.sections.forEach(s => s.slots.forEach(sl => sl.locked = false)); break;
    case "mood": Object.assign(p.mood, op.v); break;
    case "key": p.key = op.v; break;
    case "planPref": sec.planPref = op.v; break;
  }
  op.h = hashText(canonText(p));
  fx.ops.push(op);
}

function randomProject(k) {
  const p = E.newProject();
  p.mode = E.MODE_ORDER[k % 5];
  p.officeSafe = !E.MODES[p.mode].minor;
  p.key = Math.floor(rnd() * 12);
  p.bpb = rnd() < .25 ? 3 : 4;
  p.tempo = 40 + Math.floor(rnd() * 80);
  p.jazz = rnd() < .2;
  p.tie = rnd() < .8;
  p.bassTrack = rnd() < .85;
  p.leadBeats = rnd() < .5 ? 1 : 2;
  const v = () => { const r = rnd(); return r < .15 ? 0 : r > .85 ? 1 : Math.round(rnd() * 20) / 20; };
  p.mood = { wistful: v(), moving: v(), color: v(), inner: v(), surprise: v(), leadins: rnd() < .6 ? 0 : v() };
  if (rnd() < .3) p.sections.push(E.newSection("turn"));
  p.sections.forEach(s => {
    s.length = rnd() < .3 ? 4 : 8;
    s.chordBars = pick([1, 1, 1, 2, 4]);
    if (rnd() < .2) s.planPref = pick(E.allowedPlans(p, s.role));
  });
  if (p.sections.length > 4) p.arrangement.push({ id: "t" + k, sectionId: p.sections[4].id, bars: 4 });
  return p;
}

// ---- A. random projects, rolled once; every sixth then gets the lock stress run ----
for (let k = 0; k < COUNT; k++) {
  const p = randomProject(k);
  const fx = { id: `P${k}`, kind: "project", init: JSON.parse(JSON.stringify(p)), ops: [] };
  apply(fx, p, { op: "rollAll", seeds: p.sections.map(seed) });
  if (k % 6 === 0) {
    fx.kind = "locks";
    p.sections.forEach((s, si) => s.slots.forEach((sl, i) => { if (rnd() < .33) apply(fx, p, { op: "lock", sec: si, slot: i, v: true }); }));
    for (let r = 0; r < 50; r++) {
      const m = r % 5;
      if (m === 0) { apply(fx, p, { op: "mood", v: { moving: rnd(), color: rnd(), inner: rnd(), leadins: rnd() < .5 ? 0 : rnd() } }); apply(fx, p, { op: "rollAll", seeds: p.sections.map(seed) }); }
      else if (m === 1) { const si = Math.floor(rnd() * p.sections.length); apply(fx, p, { op: "planPref", sec: si, v: pick(["auto", ...E.allowedPlans(p, p.sections[si].role)]) }); apply(fx, p, { op: "roll", sec: si, seed: seed() }); }
      else if (m === 2) { apply(fx, p, { op: "key", v: Math.floor(rnd() * 12) }); apply(fx, p, { op: "roll", sec: Math.floor(rnd() * p.sections.length), seed: seed() }); }
      else { const si = Math.floor(rnd() * p.sections.length); apply(fx, p, { op: "reroll", sec: si, slot: Math.floor(rnd() * p.sections[si].slots.length), seed: seed() }); }
    }
    apply(fx, p, { op: "unlockAll" });
    for (let r = 0; r < 6; r++) { const si = Math.floor(rnd() * p.sections.length); apply(fx, p, { op: "reroll", sec: si, slot: Math.floor(rnd() * p.sections[si].slots.length), seed: seed() }); }
  } else if (k % 6 === 1) {
    // a few single re-rolls and section rolls without locks
    for (let r = 0; r < 8; r++) { const si = Math.floor(rnd() * p.sections.length);
      if (r % 2) apply(fx, p, { op: "roll", sec: si, seed: seed() }); else apply(fx, p, { op: "reroll", sec: si, slot: Math.floor(rnd() * p.sections[si].slots.length), seed: seed() }); }
  }
  finish(fx, p);
}

// ---- B. migration: two hand-built files (formats 1 and 2) and projects made by the real v1 engine ----
const s1 = { schema: 1, engine: "0.1.0", key: 0, mode: "ionian", officeSafe: true, tempo: 60, bpb: 4,
  mood: { drive: .25, jagged: .2, surprise: .35, color: .6, leadins: .25 }, leadBeats: 1, bassTrack: true,
  sections: [{ id: "a", name: "Verse", role: "home", bars: 2, slots: [
    { locked: true,  chord: { root: 0, q: "maj", ext: "6/9", iv: [0,4,7,9,14], kind: "diatonic", notes: [57,62,64,67], bass: 36 }, leadIn: null },
    { locked: false, chord: { root: 5, q: "maj", ext: "maj7", iv: [0,4,7,11], kind: "diatonic", notes: [57,60,64,65], bass: 41 },
      leadIn: { root: 4, q: "maj", ext: "", iv: [0,4,7], kind: "approach", sharp: true, notes: [56,59,64,68], bass: 40, beats: 1 } } ] }],
  arrangement: [{ id: "x", sectionId: "a", bars: 4 }] };
const s2 = { schema: 2, engine: "1.0.0", key: 5, mode: "mixolydian", officeSafe: true, jazz: false, tempo: 54, bpb: 4,
  mood: { drive: .15, jagged: .1, surprise: .25, color: .45, susp: .45, leadins: 0 }, leadBeats: 1, bassTrack: true,
  sections: [{ id: "a", name: "Verse", role: "home", chordBars: 2, reps: 2, pedal: true, loop: { id: "I-bVII", label: "I–♭VII" }, slots: [
    { locked: false, chord: { root: 5, q: "maj", ext: "add9", iv: [0,4,7,14], kind: "diatonic", notes: [57,60,65,67], bass: 41 }, beats: 8, sus: null, leadIn: null },
    { locked: true,  chord: { root: 3, q: "maj", ext: "", iv: [0,4,7], kind: "diatonic", notes: [55,58,63,67], bass: 41 }, beats: 8,
      sus: { beats: 4, notes: [55,60,63,67], held: 60, to: 58 }, leadIn: null },
    { locked: false, chord: { root: 5, q: "maj", ext: "add9", iv: [0,4,7,14], kind: "diatonic", notes: [57,60,65,67], bass: 41 }, beats: 8, sus: null, leadIn: null },
    { locked: false, chord: { root: 3, q: "maj", ext: "", iv: [0,4,7], kind: "diatonic", notes: [55,58,63,67], bass: 41 }, beats: 8,
      sus: { beats: 4, notes: [55,60,63,67], held: 60, to: 58 }, leadIn: null } ] }],
  arrangement: [{ id: "x", sectionId: "a", bars: 16 }], feedback: [{ at: "2026-11-11T00:00:00Z", verdict: "like", sectionId: "a" }] };
function migrationFixture(id, src, v1check) {
  const fx = { id, kind: "migration", init: JSON.parse(JSON.stringify(src)), ops: [] };
  const p = E.migrate(JSON.parse(JSON.stringify(src)));
  if (v1check) {       // what the v1 engine itself would have played from this file
    const ref = V1.migrate(JSON.parse(JSON.stringify(src)));
    const a = arrangementEvents(E, p), b = arrangementEvents(V1, ref);
    const ma = E.midiFile(p, E.playable(p, a), true), mb = V1.midiFile(ref, b, true);
    if (JSON.stringify(a) !== JSON.stringify(b) || Buffer.compare(Buffer.from(ma), Buffer.from(mb))) throw new Error("v2 does not reproduce v1 for " + id);
    fx.v1midi = fnv(mb);
  }
  fx.migrated = hashText(canonText(p));
  const ev = arrangementEvents(E, p);
  fx.migratedEvents = hashText(canonEvents(ev));
  fx.migratedMidi = fnv(E.midiFile(p, E.playable(p, ev), true));
  // then keep working on it: the locked slots must survive, and rolls must match
  for (let r = 0; r < 4; r++) { const si = Math.floor(rnd() * p.sections.length); apply(fx, p, { op: "roll", sec: si, seed: seed() }); }
  finish(fx, p);
}
migrationFixture("M-format1", s1, true);
migrationFixture("M-format2", s2, true);
for (let k = 0; k < 100; k++) {
  const p = V1.newProject(); p.mode = V1.MODE_ORDER[k % 5]; p.key = k % 12; p.mood.leadins = k % 3 ? 0 : .6; p.mood.susp = (k % 4) / 3;
  p.sections.forEach(s => { s.pedal = rnd() < .5; s.chordBars = pick([1, 2]); });
  V1.rollAll(p);
  if (k % 4 === 0) p.sections.forEach(s => s.slots.forEach(sl => { if (rnd() < .3) sl.locked = true; }));
  migrationFixture("M-v1-" + k, JSON.parse(JSON.stringify(p)), true);
}
out.end(() => console.log(`fixtures: ${stats.fixtures}   operations: ${stats.ops}   seed ${SEED}`));
