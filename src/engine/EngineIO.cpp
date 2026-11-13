// Events, MIDI files, storage and chord charts. Port of the second half of engine.js.
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kindred {

// ------------------------------------------------------------------ events ----
// Events for one section played for `bars` bars from `startBeat` (loops the
// section if the block is longer). The bass is merged across chords when it
// does not change, so a drone is one held note.
std::vector<Event> Engine::blockEvents(const Project& p, const Section& sec, double bars, double startBeat, const std::string* tag) const {
    std::vector<Event> ev, bassEv;
    const double total = bars * p.bpb; const int n = (int) sec.slots.size();
    if (!n) return ev;
    auto addBass = [&](double t, double len, const std::optional<int>& note) {
        if (!note || len <= 0) return;
        if (!bassEv.empty()) { Event& last = bassEv.back(); if (last.notes[0] == *note && std::fabs(last.t + last.len - t) < 1e-9) { last.len += len; return; } }
        Event e; e.t = t; e.len = len; e.notes = { *note }; e.bass = true; bassEv.push_back(e);
    };
    double t = 0; int i = 0;
    while (t < total - 1e-9) {
        const int idx = i % n; const Slot& slot = sec.slots[(size_t) idx];
        if (!slot.chord) break;
        if (!(slot.beats > 0)) break;                       // a damaged file must not loop forever
        const double len = std::min(slot.beats, total - t);
        const int nextIdx = (i + 1) % n; const bool more = t + slot.beats < total;
        const Chord* lead = more && nextIdx != 0 && sec.slots[(size_t) nextIdx].leadIn ? &*sec.slots[(size_t) nextIdx].leadIn : nullptr;
        const double lb = lead ? std::min(lead->beats > 0 ? lead->beats : 1.0, len - 1) : 0;
        const double sb = slot.sus && i > 0 ? std::max(0.0, std::min(slot.sus->beats, len - lb - 1)) : 0;
        const double tb = slot.tail && more && !(lb != 0) ? std::max(0.0, std::min(slot.tail->beats, len - sb - 1)) : 0;
        const double T = startBeat + t;
        Event base; base.sec = sec.id; base.idx = idx; if (tag) { base.tag = *tag; base.hasTag = true; }
        if (sb > 0) { Event e = base; e.t = T; e.len = sb; e.notes = slot.sus->notes; e.sus = true; ev.push_back(e); }
        { Event e = base; e.t = T + sb; e.len = len - lb - sb - tb; e.notes = slot.chord->notes; ev.push_back(e); }
        if (tb > 0) { Event e = base; e.t = T + len - tb; e.len = tb; e.notes = slot.tail->notes; e.tail = true; ev.push_back(e); }
        addBass(T, len - lb, slot.chord->bass);
        if (lb > 0) { Event e = base; e.t = T + len - lb; e.len = lb; e.notes = lead->notes; e.lead = true; ev.push_back(e); addBass(T + len - lb, lb, lead->bass); }
        t += slot.beats; i++;
    }
    ev.insert(ev.end(), bassEv.begin(), bassEv.end());
    return ev;
}
std::vector<Event> Engine::arrangementEvents(const Project& p, double* totalBeats) const {
    std::vector<Event> ev; double beat = 0;
    for (auto& b : p.arrangement) {
        const Section* s = p.section(b.sectionId); if (!s) continue;
        auto e = blockEvents(p, *s, b.bars, beat, &b.id);
        ev.insert(ev.end(), e.begin(), e.end());
        beat += (double) b.bars * p.bpb;
    }
    if (totalBeats) *totalBeats = beat;
    return ev;
}
// Turns chord events into single notes and joins a note that continues at the
// same pitch into the next chord, within one block, so held notes are not struck again.
std::vector<Event> Engine::tieEvents(const std::vector<Event>& events) {
    std::vector<Event> out;
    std::vector<const Event*> sorted;
    for (auto& e : events) if (!e.bass) sorted.push_back(&e);
    std::stable_sort(sorted.begin(), sorted.end(), [](const Event* a, const Event* b) { return a->t < b->t; });
    std::vector<std::pair<std::string, size_t>> open;       // key -> index in out
    for (auto* e : sorted) for (int note : e->notes) {
        const std::string k = (e->hasTag ? e->tag : std::string()) + ":" + std::to_string(note);
        auto it = std::find_if(open.begin(), open.end(), [&](auto& o) { return o.first == k; });
        if (it != open.end() && std::fabs(out[it->second].t + out[it->second].len - e->t) < 1e-9) { out[it->second].len += e->len; continue; }
        Event x; x.t = e->t; x.len = e->len; x.notes = { note }; x.tag = e->tag; x.hasTag = e->hasTag;
        if (it != open.end()) it->second = out.size(); else open.push_back({ k, out.size() });
        out.push_back(x);
    }
    for (auto& e : events) if (e.bass) out.push_back(e);
    return out;
}

// --------------------------------------------------------------- MIDI file ----
static void vlq(std::vector<uint8_t>& out, uint32_t n) {
    uint8_t b[5]; int k = 0; b[k++] = (uint8_t) (n & 0x7f);
    while ((n >>= 7)) b[k++] = (uint8_t) ((n & 0x7f) | 0x80);
    while (k) out.push_back(b[--k]);
}
namespace { struct TrackEvent { long tick; int order; std::vector<uint8_t> data; }; }
static std::vector<uint8_t> track(std::vector<TrackEvent> events, const std::string& name) {
    std::stable_sort(events.begin(), events.end(), [](const TrackEvent& a, const TrackEvent& b) { return a.tick != b.tick ? a.tick < b.tick : a.order < b.order; });
    std::vector<uint8_t> bytes{ 0, 0xff, 0x03 };
    vlq(bytes, (uint32_t) name.size()); bytes.insert(bytes.end(), name.begin(), name.end());
    long last = 0;
    for (auto& e : events) { vlq(bytes, (uint32_t) (e.tick - last)); bytes.insert(bytes.end(), e.data.begin(), e.data.end()); last = e.tick; }
    for (uint8_t b : { 0, 0xff, 0x2f, 0 }) bytes.push_back(b);
    const uint32_t len = (uint32_t) bytes.size();
    std::vector<uint8_t> out{ 0x4d, 0x54, 0x72, 0x6b, (uint8_t) (len >> 24), (uint8_t) (len >> 16), (uint8_t) (len >> 8), (uint8_t) len };
    out.insert(out.end(), bytes.begin(), bytes.end());
    return out;
}
static long jsRound(double x) { return (long) std::floor(x + 0.5); }
std::vector<uint8_t> Engine::midiFile(const Project& p, const std::vector<Event>& events, bool bassSeparate) const {
    const long tempo = jsRound(60000000.0 / p.tempo);
    std::vector<TrackEvent> meta{
        { 0, 0, { 0xff, 0x51, 3, (uint8_t) ((tempo >> 16) & 255), (uint8_t) ((tempo >> 8) & 255), (uint8_t) (tempo & 255) } },
        { 0, 0, { 0xff, 0x58, 4, (uint8_t) p.bpb, 2, 24, 8 } } };
    std::vector<TrackEvent> chords, bass;
    for (auto& e : events) {
        const long on = jsRound(e.t * kPPQ), off = jsRound((e.t + e.len) * kPPQ) - 1;
        const bool sep = e.bass && bassSeparate;
        const uint8_t ch = sep ? 1 : 0, vel = (uint8_t) (e.bass ? st->rules.bassVelocity : st->rules.chordVelocity);
        auto& dst = sep ? bass : chords;
        for (int n : e.notes) { dst.push_back({ on, 1, { (uint8_t) (0x90 | ch), (uint8_t) n, vel } }); dst.push_back({ off, 0, { (uint8_t) (0x80 | ch), (uint8_t) n, 0 } }); }
    }
    std::vector<std::vector<uint8_t>> tracks{ track(meta, "Tempo"), track(chords, "Chords") };
    if (bassSeparate) tracks.push_back(track(bass, "Bass"));
    std::vector<uint8_t> out{ 0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0, (uint8_t) tracks.size(), (uint8_t) (kPPQ >> 8), (uint8_t) (kPPQ & 255) };
    for (auto& t : tracks) out.insert(out.end(), t.begin(), t.end());
    return out;
}

std::vector<uint8_t> Engine::midiFileBassOnly(const Project& p, const std::vector<Event>& events) const {
    const long tempo = jsRound(60000000.0 / p.tempo);
    std::vector<TrackEvent> meta{
        { 0, 0, { 0xff, 0x51, 3, (uint8_t) ((tempo >> 16) & 255), (uint8_t) ((tempo >> 8) & 255), (uint8_t) (tempo & 255) } },
        { 0, 0, { 0xff, 0x58, 4, (uint8_t) p.bpb, 2, 24, 8 } } };
    std::vector<TrackEvent> bass;
    for (auto& e : events) {
        if (!e.bass) continue;
        const long on = jsRound(e.t * kPPQ), off = jsRound((e.t + e.len) * kPPQ) - 1;
        for (int n : e.notes) { bass.push_back({ on, 1, { 0x91, (uint8_t) n, (uint8_t) st->rules.bassVelocity } }); bass.push_back({ off, 0, { 0x81, (uint8_t) n, 0 } }); }
    }
    std::vector<std::vector<uint8_t>> tracks{ track(meta, "Tempo"), track(bass, "Bass") };
    std::vector<uint8_t> out{ 0x4d, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0, (uint8_t) tracks.size(), (uint8_t) (kPPQ >> 8), (uint8_t) (kPPQ & 255) };
    for (auto& t : tracks) out.insert(out.end(), t.begin(), t.end());
    return out;
}

// ----------------------------------------------------------------- storage ----
static Json intArray(const std::vector<int>& v) { Json a = Json::array(); for (int x : v) a.push(Json(x)); return a; }
static std::vector<int> ints(const Json& j) { std::vector<int> v; for (auto& x : j.items()) v.push_back(x.integer()); return v; }

static Json chordToJson(const Chord& c, bool lead) {
    Json o = Json::object();
    o.set("root", c.root); o.set("q", c.q); o.set("ext", c.ext); o.set("iv", intArray(c.iv)); o.set("kind", c.kind);
    if (lead || c.sharp) o.set("sharp", c.sharp);
    o.set("notes", intArray(c.notes)); o.set("bass", c.bass ? Json(*c.bass) : Json());
    if (lead) o.set("beats", c.beats);
    return o;
}
static Chord chordFromJson(const Json& o) {
    Chord c; c.root = mod12(o["root"].integer()); c.q = o["q"].stringOr("maj"); c.ext = o["ext"].stringOr("");
    c.iv = ints(o["iv"]); if (c.iv.empty()) c.iv = { 0, 4, 7 };
    c.kind = o["kind"].stringOr("diatonic"); if (c.kind.empty()) c.kind = "diatonic";
    for (int n : ints(o["notes"])) if (n >= 0 && n <= 127) c.notes.push_back(n);
    if (o["bass"].isNumber()) { const int b = o["bass"].integer(); if (b >= 0 && b <= 127) c.bass = b; }
    c.sharp = o["sharp"].boolean(); c.beats = o["beats"].number(0);
    return c;
}
static Json innerToJson(const Inner& s, const char* fromKey) {
    Json o = Json::object(); o.set("beats", s.beats); o.set("notes", intArray(s.notes)); o.set(fromKey, s.from); o.set("to", s.to); return o;
}
static Inner innerFromJson(const Json& o, const char* fromKey) {
    Inner s; s.beats = o["beats"].number(1); s.from = o[fromKey].integer(); s.to = o["to"].integer();
    for (int n : ints(o["notes"])) if (n >= 0 && n <= 127) s.notes.push_back(n);
    return s;
}

Json Engine::toJson(const Project& p) {
    Json j = Json::object();
    j.set("app", "Kindred"); j.set("schema", kSchema); j.set("engine", kEngineVersion); j.set("style", p.style);
    j.set("key", p.key); j.set("mode", p.mode); j.set("majorModesOnly", p.majorModesOnly); j.set("extended", p.extended); j.set("tie", p.tie);
    j.set("tempo", p.tempo); j.set("bpb", p.bpb);
    Json mood = Json::object();
    mood.set("wistful", p.mood.wistful); mood.set("moving", p.mood.moving); mood.set("color", p.mood.color); mood.set("inner", p.mood.inner);
    mood.set("surprise", p.mood.surprise); mood.set("leadins", p.mood.leadins); mood.set("edge", p.mood.edge);
    j.set("mood", mood); j.set("leadBeats", p.leadBeats); j.set("bassTrack", p.bassTrack);
    Json secs = Json::array();
    for (auto& s : p.sections) {
        Json o = Json::object();
        o.set("id", s.id); o.set("name", s.name); o.set("role", s.role); o.set("chordBars", s.chordBars); o.set("length", s.length); o.set("planPref", s.planPref);
        if (s.plan) { Json pl = Json::object(); pl.set("id", s.plan->id); pl.set("label", s.plan->label); o.set("plan", pl); } else o.set("plan", Json());
        o.set("from", s.from ? Json(*s.from) : Json());
        Json slots = Json::array();
        for (auto& sl : s.slots) {
            Json x = Json::object();
            x.set("locked", sl.locked); x.set("chord", sl.chord ? chordToJson(*sl.chord, false) : Json()); x.set("beats", sl.beats);
            x.set("sus", sl.sus ? innerToJson(*sl.sus, "held") : Json()); x.set("tail", sl.tail ? innerToJson(*sl.tail, "from") : Json());
            x.set("leadIn", sl.leadIn ? chordToJson(*sl.leadIn, true) : Json());
            slots.push(x);
        }
        o.set("slots", slots);
        for (auto& m : s.extra.members()) if (!o.has(m.first)) o.set(m.first, m.second);
        secs.push(o);
    }
    j.set("sections", secs);
    Json arr = Json::array();
    for (auto& b : p.arrangement) { Json o = Json::object(); o.set("id", b.id); o.set("sectionId", b.sectionId); o.set("bars", b.bars); arr.push(o); }
    j.set("arrangement", arr); j.set("feedback", p.feedback.isArray() ? p.feedback : Json::array());
    for (auto& m : p.extra.members()) if (!j.has(m.first)) j.set(m.first, m.second);
    return j;
}

// Older files are converted in place. Chords, notes, bass notes and beats are
// never changed here; only missing settings are filled in.
void Engine::migrateLegacy(Json& p) {
    if (!p.isObject() || !p["sections"].isArray()) throw std::runtime_error("This file is not a Kindred or Chord Engine project.");
    int schema = p["schema"].integer(0);
    if (schema > kSchema) throw std::runtime_error("This project was saved by a newer version (format " + std::to_string(schema) + ").");
    auto each = [](Json& arrayOwner, const char* key, const std::function<void(Json&)>& fn) {
        for (auto& m : arrayOwner.members()) if (m.first == key && m.second.isArray()) for (auto& x : m.second.items()) if (x.isObject()) fn(x);
    };
    const int bpb = std::max(1, p["bpb"].integer(4));
    if (schema < 2) {
        // Format 1: one chord per bar, no drone or suspensions.
        each(p, "sections", [&](Json& s) {
            s.set("chordBars", 1); s.set("reps", 1); s.set("pedal", false); s.set("loop", Json());
            each(s, "slots", [&](Json& sl) { sl.set("beats", bpb); sl.set("sus", Json()); });
        });
        Json mood = p["mood"].isObject() ? p["mood"] : Json::object();
        mood.set("susp", 0); p.set("mood", mood); p.set("jazz", true); p.set("feedback", Json::array()); p.set("schema", 2); schema = 2;
    }
    if (schema < 3) {
        // Format 2: loops with a per-section pedal switch. Old files play as they
        // did: chords are struck again at every change (no ties).
        const Json old = p["mood"].isObject() ? p["mood"] : Json::object();
        Json mood = Json::object();
        mood.set("wistful", .35); mood.set("moving", .5); mood.set("color", .5); mood.set("inner", .5); mood.set("surprise", .2); mood.set("leadins", 0);
        for (auto& m : old.members()) mood.set(m.first, m.second);
        mood.set("inner", old.has("susp") && !old["susp"].isNull() ? old["susp"] : Json(.5));
        p.set("mood", mood); p.set("tie", false);
        each(p, "sections", [&](Json& s) {
            s.set("length", s["slots"].size() <= 4 ? 4 : 8); s.set("planPref", "auto"); s.set("plan", Json()); s.set("from", Json());
            if (!s["chordBars"].boolean()) s.set("chordBars", 1);
            each(s, "slots", [&](Json& sl) { if (!sl.has("tail")) sl.set("tail", Json()); if (!sl.has("sus")) sl.set("sus", Json()); if (sl["beats"].isNull()) sl.set("beats", bpb); });
        });
        if (!p["feedback"].boolean()) p.set("feedback", Json::array());
        p.set("schema", 3);
    }
}

Project Engine::fromJson(Json j) {
    migrateLegacy(j);
    const int schema = j["schema"].integer(3);
    Project p;
    p.style = j["style"].stringOr("warm-ambient");
    p.key = mod12(j["key"].integer(0)); p.mode = j["mode"].stringOr("ionian");
    // Formats 1 to 3 called these officeSafe and jazz.
    p.majorModesOnly = schema >= 4 ? j["majorModesOnly"].boolean() : j["officeSafe"].boolean();
    p.extended = schema >= 4 ? j["extended"].boolean() : j["jazz"].boolean();
    p.tie = j.has("tie") ? j["tie"].boolean() : true;
    p.tempo = j["tempo"].number(60); if (!(p.tempo >= 1 && p.tempo <= 999)) p.tempo = 60;
    p.bpb = std::max(1, std::min(16, j["bpb"].integer(4)));
    const Json& mood = j["mood"];
    for (const char* k : { "wistful", "moving", "color", "inner", "surprise", "leadins", "edge" })
        if (mood[k].isNumber()) *p.mood.byId(k) = std::max(0.0, std::min(1.0, mood[k].number()));
    p.leadBeats = std::max(1, j["leadBeats"].integer(1)); p.bassTrack = j.has("bassTrack") ? j["bassTrack"].boolean() : true;
    static const char* knownSec[] = { "id", "name", "role", "chordBars", "length", "planPref", "plan", "from", "slots", "bars" };
    int autoId = 0;
    for (auto& o : j["sections"].items()) {
        if (!o.isObject()) continue;
        Section s; s.id = o["id"].stringOr(""); if (s.id.empty()) s.id = "s" + std::to_string(++autoId);
        s.role = o["role"].stringOr("home"); s.name = o["name"].stringOr(s.role);
        s.chordBars = std::max(1, std::min(8, o["chordBars"].integer(1))); s.length = o["length"].integer(8) == 4 ? 4 : 8;
        s.planPref = o["planPref"].stringOr("auto"); if (s.planPref.empty()) s.planPref = "auto";
        if (o["plan"].isObject()) s.plan = PlanRef{ o["plan"]["id"].stringOr(""), o["plan"]["label"].stringOr("") };
        if (o["from"].isString()) s.from = o["from"].string();
        for (auto& x : o["slots"].items()) {
            Slot sl; sl.locked = x["locked"].boolean();
            if (x["chord"].isObject()) sl.chord = chordFromJson(x["chord"]);
            sl.beats = x["beats"].number(p.bpb); if (!(sl.beats > 0)) sl.beats = p.bpb;
            if (x["sus"].isObject()) sl.sus = innerFromJson(x["sus"], "held");
            if (x["tail"].isObject()) sl.tail = innerFromJson(x["tail"], "from");
            if (x["leadIn"].isObject()) sl.leadIn = chordFromJson(x["leadIn"]);
            s.slots.push_back(sl);
        }
        for (auto& m : o.members()) { bool known = false; for (auto* k : knownSec) if (m.first == k) known = true; if (!known) s.extra.set(m.first, m.second); }
        p.sections.push_back(s);
    }
    autoId = 0;
    for (auto& o : j["arrangement"].items()) {
        if (!o.isObject()) continue;
        Block b; b.id = o["id"].stringOr(""); if (b.id.empty()) b.id = "b" + std::to_string(++autoId);
        b.sectionId = o["sectionId"].stringOr(""); b.bars = std::max(1, std::min(999, o["bars"].integer(8)));
        p.arrangement.push_back(b);
    }
    p.feedback = j["feedback"].isArray() ? j["feedback"] : Json::array();
    static const char* known[] = { "app", "schema", "engine", "style", "key", "mode", "majorModesOnly", "officeSafe", "extended", "jazz", "tie", "tempo", "bpb", "mood",
                                   "leadBeats", "bassTrack", "sections", "arrangement", "feedback" };
    for (auto& m : j.members()) { bool k = false; for (auto* n : known) if (m.first == n) k = true; if (!k) p.extra.set(m.first, m.second); }
    return p;
}

// ------------------------------------------------------------------- chart ----
// "C [F>E]" = F is held over from the chord before, then falls to E.
// "C [~B]"  = one voice passes through B on its way to the next chord.
static size_t utf8Len(const std::string& s) { size_t n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++; return n; }
static std::string padEnd(const std::string& s, size_t w) { const size_t n = utf8Len(s); return n >= w ? s : s + std::string(w - n, ' '); }
std::string Engine::slotText(const Project& p, const Section& sec, size_t i) const {
    const Slot& sl = sec.slots[i];
    if (!sl.chord) return "–";
    std::string s = chordName(*sl.chord, p.key);
    if (sl.sus) s += " [" + pcName(sl.sus->from, p.key) + ">" + pcName(sl.sus->to, p.key) + "]";
    if (sl.tail) s += " [~" + pcName(sl.tail->to, p.key) + "]";
    const double b = sl.beats / p.bpb;
    if (b != 1) s += " (" + Json::numberToString(b) + ")";
    if (i + 1 < sec.slots.size() && sec.slots[i + 1].leadIn) s += " ‹" + chordName(*sec.slots[i + 1].leadIn, p.key) + "›";
    return s;
}
std::string Engine::sectionChart(const Project& p, const Section& sec) const {
    std::vector<std::string> chords, bass; std::vector<size_t> w;
    for (size_t i = 0; i < sec.slots.size(); i++) {
        chords.push_back(slotText(p, sec, i));
        const auto& sl = sec.slots[i];
        bass.push_back(sl.chord && sl.chord->bass ? pcName(*sl.chord->bass, p.key) : "–");
        w.push_back(std::max(utf8Len(chords.back()), utf8Len(bass.back())));
    }
    std::string head = sec.name + (sec.plan ? " · " + sec.plan->label : "") + (sec.from && sec.role != "home" ? " · from " + *sec.from : "");
    std::string a = "  | ", b = "  | ";
    for (size_t i = 0; i < chords.size(); i++) { a += padEnd(chords[i], w[i]) + (i + 1 < chords.size() ? " | " : ""); b += padEnd(bass[i], w[i]) + (i + 1 < chords.size() ? " | " : ""); }
    return head + "\n" + a + " |\n" + b + " |  bass";
}
std::string Engine::chartText(const Project& p) const {
    std::string arr;
    for (size_t i = 0; i < p.arrangement.size(); i++) {
        const Section* s = p.section(p.arrangement[i].sectionId);
        arr += (i ? ", " : "") + (s ? s->name : std::string("?")) + " ×" + std::to_string(p.arrangement[i].bars);
    }
    std::string out = "Key " + keyName(p.key) + " " + st->modeOr(p.mode).name + " · " + Json::numberToString(p.tempo) + " BPM · " + std::to_string(p.bpb) + "/4\n\n";
    for (size_t i = 0; i < p.sections.size(); i++) out += (i ? "\n\n" : "") + sectionChart(p, p.sections[i]);
    return out + "\n\nArrangement (bars): " + arr;
}

// ------------------------------------------------------------- comparisons ----
static std::string num(double x) {
    if (x == std::floor(x)) { char b[32]; std::snprintf(b, sizeof b, "%.0f", x); return b; }
    char b[40]; std::snprintf(b, sizeof b, "%.6f", x); return b;
}
static std::string join(const std::vector<int>& v) { std::string s; for (size_t i = 0; i < v.size(); i++) s += (i ? "." : "") + std::to_string(v[i]); return s; }
static std::string canonChord(const std::optional<Chord>& c, bool lead) {
    if (!c) return "-";
    return std::to_string(c->root) + "," + c->q + "," + c->ext + "," + join(c->iv) + "," + c->kind + "," + join(c->notes) + "," + (c->bass ? std::to_string(*c->bass) : "-")
        + "," + (c->sharp ? "1" : "0") + (lead ? "," + num(c->beats) : "");
}
static std::string canonInner(const std::optional<Inner>& s) {
    if (!s) return "-";
    return num(s->beats) + "," + join(s->notes) + "," + std::to_string(s->from) + "," + std::to_string(s->to);
}
std::string canonText(const Project& p) {
    std::string out = "K" + std::to_string(p.key) + " M" + p.mode + "\n";
    for (auto& s : p.sections) {
        out += "S " + s.role + " " + (s.plan ? s.plan->id : "-") + " " + (s.from ? *s.from : "-") + " n=" + std::to_string(s.slots.size()) + "\n";
        for (auto& sl : s.slots)
            out += std::string(" L") + (sl.locked ? "1" : "0") + " b" + num(sl.beats) + " C[" + canonChord(sl.chord, false) + "] S[" + canonInner(sl.sus) + "] T[" + canonInner(sl.tail)
                + "] I[" + canonChord(sl.leadIn, true) + "]\n";
    }
    return out;
}
std::string canonEvents(const std::vector<Event>& ev) {
    std::string out;
    for (auto& e : ev) {
        out += num(e.t) + " " + num(e.len) + " " + join(e.notes) + " " + (e.bass ? "b" : "") + (e.sus ? "s" : "") + (e.tail ? "t" : "") + (e.lead ? "l" : "") + " "
            + (e.sec.empty() ? "-" : e.sec) + " " + std::to_string(e.idx) + " " + (e.hasTag ? e.tag : "-") + "\n";
    }
    return out;
}
uint64_t fnv1a(const std::string& s) { uint64_t h = 0xcbf29ce484222325ull; for (unsigned char c : s) { h ^= c; h *= 0x100000001b3ull; } return h; }
uint64_t fnv1a(const std::vector<uint8_t>& b) { uint64_t h = 0xcbf29ce484222325ull; for (uint8_t c : b) { h ^= c; h *= 0x100000001b3ull; } return h; }

}  // namespace kindred
