// Golden equivalence: replays the operations recorded by gen-fixtures.js (which
// ran the JavaScript prototype) in the C++ engine and compares every state hash,
// the final project text, the event lists and the MIDI file bytes.
//   KindredGolden <style.json> <fixtures.jsonl> [-v]
#include "../../src/engine/Engine.h"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace kindred;

static std::string hex(uint64_t h) { char b[20]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long) h); return b; }
static std::string readFile(const std::string& path) { std::ifstream f(path, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

static void firstDiff(const std::string& a, const std::string& b) {
    std::istringstream sa(a), sb(b); std::string la, lb; int line = 0;
    while (true) {
        const bool ha = (bool) std::getline(sa, la), hb = (bool) std::getline(sb, lb); line++;
        if (!ha && !hb) return;
        if (!ha || !hb || la != lb) { std::printf("      line %d\n      js : %s\n      c++: %s\n", line, ha ? la.c_str() : "<end>", hb ? lb.c_str() : "<end>"); return; }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: KindredGolden <style.json> <fixtures.jsonl>\n"); return 2; }
    // By default the style's fixes to the prototype are switched off, so the comparison is against the
    // prototype exactly. With --shipped the style runs as shipped, to count how many results the fixes change.
    const bool shipped = argc > 3 && std::string(argv[3]) == "--shipped";
    Json styleJson = Json::parse(readFile(argv[1]));
    if (!shipped) { Json rules = styleJson["rules"]; rules.set("seamKeepsNeighbor", false); styleJson.set("rules", rules); }
    Engine eng(Style::fromJson(styleJson));
    std::printf(shipped ? "mode: style as shipped (prototype fixes ON)\n" : "mode: prototype-exact (prototype fixes OFF)\n");
    std::ifstream in(argv[2]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    long fixtures = 0, ops = 0, compares = 0, bad = 0, badFixtures = 0, rolls = 0;
    long byKind[3] = {0, 0, 0};
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const Json fx = Json::parse(line);
        const std::string id = fx["id"].string(), kind = fx["kind"].string();
        fixtures++; byKind[kind == "project" ? 0 : kind == "locks" ? 1 : 2]++;
        long before = bad;
        auto check = [&](bool ok, const char* what) { compares++; if (!ok) { bad++; if (!shipped && bad - before <= 3) std::printf("  MISMATCH %s: %s\n", id.c_str(), what); } return ok; };
        Project p;
        try { p = Engine::fromJson(fx["init"]); } catch (const std::exception& e) { check(false, e.what()); badFixtures++; continue; }
        if (kind == "migration") {
            check(hex(fnv1a(canonText(p))) == fx["migrated"].string(), "project after migration");
            const auto ev = eng.arrangementEvents(p);
            check(hex(fnv1a(canonEvents(ev))) == fx["migratedEvents"].string(), "events after migration");
            const std::string mh = hex(fnv1a(eng.midiFile(p, Engine::playable(p, ev), true)));
            check(mh == fx["migratedMidi"].string(), "MIDI bytes after migration");
            if (fx.has("v1midi")) check(mh == fx["v1midi"].string(), "MIDI bytes equal the v1 engine's own output");
        }
        bool diverged = false;
        for (auto& op : fx["ops"].items()) {
            ops++;
            const std::string o = op["op"].string();
            const int si = op["sec"].integer(-1);
            Section* sec = si >= 0 && si < (int) p.sections.size() ? &p.sections[(size_t) si] : nullptr;
            if (o == "rollAll") { size_t k = 0; const auto& seeds = op["seeds"].items(); eng.rollAll(p, [&]() { return (uint32_t) seeds[k++].number(); }); rolls += (long) seeds.size(); }
            else if (o == "roll") { eng.rollSection(p, *sec, nullptr, (uint32_t) op["seed"].number()); rolls++; }
            else if (o == "reroll") { std::vector<int> only{ op["slot"].integer() }; eng.rollSection(p, *sec, &only, (uint32_t) op["seed"].number()); rolls++; }
            else if (o == "lock") sec->slots[(size_t) op["slot"].integer()].locked = op["v"].boolean();
            else if (o == "unlockAll") { for (auto& s : p.sections) for (auto& sl : s.slots) sl.locked = false; }
            else if (o == "mood") { for (auto& m : op["v"].members()) if (double* v = p.mood.byId(m.first)) *v = m.second.number(); }
            else if (o == "key") p.key = op["v"].integer();
            else if (o == "planPref") sec->planPref = op["v"].string();
            if (!diverged && !check(hex(fnv1a(canonText(p))) == op["h"].string(), ("state after operation " + std::to_string(ops) + " (" + o + ")").c_str())) {
                diverged = true;
                if (!shipped && bad - before <= 3) std::printf("      (run the same fixture in node to see the JS state; operation: %s)\n", op.dump().c_str());
            }
        }
        const std::string canon = canonText(p);
        if (!check(canon == fx["canon"].string(), "final project") && !shipped && bad - before <= 3) firstDiff(fx["canon"].string(), canon);
        const auto ev = eng.arrangementEvents(p);
        check(hex(fnv1a(canonEvents(ev))) == fx["events"].string(), "arrangement events");
        check(hex(fnv1a(canonEvents(Engine::tieEvents(ev)))) == fx["tied"].string(), "tied events");
        const auto midi = eng.midiFile(p, Engine::playable(p, ev), p.bassTrack);
        check((int) midi.size() == fx["midiLen"].integer() && hex(fnv1a(midi)) == fx["midi"].string(), "arrangement MIDI bytes");
        if (fx.has("secMidi")) {
            const Section& s0 = p.sections[0];
            const auto sm = eng.midiFile(p, Engine::playable(p, eng.blockEvents(p, s0, Engine::sectionBars(p, s0), 0, nullptr)), p.bassTrack);
            check((int) sm.size() == fx["secMidiLen"].integer() && hex(fnv1a(sm)) == fx["secMidi"].string(), "section MIDI bytes");
        }
        check(hex(fnv1a(eng.chartText(p))) == fx["chart"].string(), "chord chart text");
        // Storage: a save and reload must give the same project, events and bytes.
        const Project back = Engine::fromJson(Json::parse(Engine::toJson(p).dump()));
        check(canonText(back) == canon && Engine::toJson(back) == Engine::toJson(p), "save and reload is exact");
        check(eng.midiFile(back, Engine::playable(back, eng.arrangementEvents(back)), back.bassTrack) == midi, "MIDI bytes identical after reload");
        if (bad != before) badFixtures++;
    }
    std::printf("fixtures: %ld (projects %ld, lock runs %ld, migration %ld)   operations: %ld   rolls: %ld   comparisons: %ld   mismatches: %ld in %ld fixtures\n",
                fixtures, byKind[0], byKind[1], byKind[2], ops, rolls, compares, bad, badFixtures);
    if (shipped) { std::printf("with the fixes on, %ld of %ld fixtures differ from the prototype\n", badFixtures, fixtures); return 0; }
    std::printf(bad ? "GOLDEN FAILED\n" : "golden equivalence: all comparisons equal\n");
    return bad ? 1 : 0;
}
