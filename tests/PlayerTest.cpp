// Stress test of the MIDI scheduler (src/plugin/Player.h) without a host:
// random projects, random block sizes, tempo changes, stops, locates, loops,
// new timelines and new settings arriving in the middle of a chord.
// A MIDI monitor checks every message: a note-on for a pitch that is already
// on, a note-off for a pitch that is not on, and any note still on after a
// stop are errors.
//   KindredPlayerTest <style.json> [scenarios] [seed]
#include "../src/plugin/Player.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include <fstream>
#include <sstream>

using namespace kindred;

struct Monitor {
    bool on[16][128] = {}; long ons = 0, offs = 0, overlaps = 0, orphans = 0, cc = 0, order = 0; int lastSample = 0;
    void operator()(int sample, uint8_t st, uint8_t d1, uint8_t d2) {
        if (sample < lastSample) order++;
        lastSample = sample;
        const int ch = st & 15, hi = st & 0xf0;
        if (hi == 0x90 && d2 > 0) { if (on[ch][d1]) overlaps++; on[ch][d1] = true; ons++; }
        else if (hi == 0x80 || hi == 0x90) { if (!on[ch][d1]) orphans++; on[ch][d1] = false; offs++; }
        else if (hi == 0xB0) cc++;
    }
    int sounding() const { int n = 0; for (auto& c : on) for (bool b : c) n += b; return n; }
    void newBlock() { lastSample = 0; }
};

static std::string readFile(const std::string& path) { std::ifstream f(path, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: KindredPlayerTest <style.json> [scenarios] [seed]\n"); return 2; }
    const int N = argc > 2 ? std::atoi(argv[2]) : 2000; const uint32_t SEED = argc > 3 ? (uint32_t) std::atoll(argv[3]) : 7u;
    Engine eng(Style::fromText(readFile(argv[1])));
    Rng rnd(SEED); int idn = 0;
    auto ids = [&] { return "i" + std::to_string(++idn); };
    auto seed = [&] { return (uint32_t) (rnd.next() * 4294967296.0); };
    auto ri = [&](int lo, int hi) { return lo + (int) std::floor(rnd.next() * (hi - lo + 1)); };
    uint64_t tlId = 0;
    auto makeTimeline = [&](Project& p, bool section, PlayParams pp) {
        Timeline tl; tl.id = ++tlId;
        std::vector<Event> ev;
        if (section) { const Section& s = p.sections[(size_t) ri(0, (int) p.sections.size() - 1)]; const double bars = Engine::sectionBars(p, s); ev = eng.blockEvents(p, s, bars, 0, nullptr); tl.total = bars * p.bpb; }
        else ev = eng.arrangementEvents(p, &tl.total);
        buildNotes(tl, Engine::playable(p, ev), pp);
        return tl;
    };
    auto randomParams = [&] {
        PlayParams pp; pp.part = ri(0, 2); pp.chordCh = ri(0, 3); pp.bassCh = rnd.next() < .3 ? pp.chordCh : ri(0, 3);
        pp.chordOct = ri(-2, 2); pp.bassOct = ri(-1, 2); pp.chordVel = ri(1, 127); pp.bassVel = ri(1, 127);
        pp.humanize = rnd.next() < .5 ? 0 : rnd.next(); pp.strum = rnd.next() < .5 ? 0 : rnd.next(); pp.gate = rnd.next() < .5 ? 1 : .3 + .7 * rnd.next();
        return pp;
    };
    long blocks = 0, stops = 0, jumps = 0, swaps = 0, errors = 0, stuckAfterStop = 0, totalOns = 0, totalOffs = 0, wraps = 0;
    long exactRuns = 0, exactNotes = 0, exactMissing = 0, exactLate = 0;
    for (int sc = 0; sc < N; sc++) {
        Project p = eng.newProject(ids);
        p.mode = eng.style().modes[(size_t) sc % eng.style().modes.size()].id; p.key = ri(0, 11); p.bpb = ri(2, 7); p.tie = rnd.next() < .7;
        p.mood.leadins = rnd.next() < .5 ? 0 : rnd.next(); p.mood.inner = rnd.next(); p.mood.moving = rnd.next();
        for (auto& s : p.sections) { s.length = rnd.next() < .3 ? 4 : 8; s.chordBars = ri(1, 2); }
        for (auto& b : p.arrangement) b.bars = ri(1, 12);
        eng.rollAll(p, seed);
        PlayParams pp = randomParams();
        if (sc % 4 == 0) pp = PlayParams();
        bool section = rnd.next() < .4;
        Timeline tl = makeTimeline(p, section, pp), tl2;
        Player player; Monitor mon;
        const double sr = 44100.0 * (1 + ri(0, 3));
        double bpm = 30 + rnd.next() * 210, pos = 0; bool loop = rnd.next() < .6, playing = true;
        const Timeline* cur = &tl;

        if (sc % 5 == 0) {
            // Straight run, no jumps: every note of the timeline must start exactly once, within one sample of its time.
            Player pl; struct Rec { int ch, pitch; double beat; }; std::vector<Rec> got; Monitor m2;
            const int bs = ri(32, 1024); double q = 0; const double bps = bpm / 60 / sr;
            while (q < tl.total + 4) {
                pl.process(&tl, q, bps, bs, false, [&](int sample, uint8_t st, uint8_t d1, uint8_t d2) { m2(sample, st, d1, d2); if ((st & 0xf0) == 0x90) got.push_back({ st & 15, d1, q + sample * bps }); });
                m2.newBlock(); q += bs * bps;
            }
            exactRuns++; exactNotes += (long) tl.notes.size();
            if (got.size() != tl.notes.size()) exactMissing += std::labs((long) got.size() - (long) tl.notes.size());
            else for (size_t i = 0; i < got.size(); i++) if (std::fabs(got[i].beat - tl.notes[i].on) > 1.5 * bps || got[i].pitch != tl.notes[i].pitch) exactLate++;
            if (m2.sounding() || m2.overlaps || m2.orphans || m2.order) errors++;
        }

        for (int step = 0; step < 400; step++) {
            const int bs = rnd.next() < .1 ? ri(1, 16) : ri(32, 2048);
            const double bps = bpm / 60 / sr;
            const double r = rnd.next();
            if (r < .03) {                                   // transport stop
                player.allOff(0, mon); mon.newBlock(); stops++; playing = false;
                if (mon.sounding()) { stuckAfterStop++; errors++; }
            } else if (r < .06) { pos = rnd.next() * cur->total * 1.2 - (rnd.next() < .1 ? 3 : 0); jumps++; playing = true; }      // locate (sometimes before the start)
            else if (r < .09) { bpm = 30 + rnd.next() * 210; }                                         // tempo change
            else if (r < .12) { if (rnd.next() < .5) eng.rollSection(p, p.sections[(size_t) ri(0, (int) p.sections.size() - 1)], nullptr, seed()); else pp = randomParams();
                                section = rnd.next() < .4; tl2 = makeTimeline(p, section, pp); std::swap(tl, tl2); cur = &tl; swaps++; }   // new music or settings mid-chord
            else if (r < .14) { loop = !loop; }
            else if (r < .16) { pos += (rnd.next() - .5) * 0.01; }                                   // clock noise from the host
            else if (r < .17) { pos = cur->total - bps * bs * rnd.next(); }                           // land just before the end
            if (!playing && r >= .5) playing = true;
            if (!playing) continue;
            const double before = pos;
            player.process(cur, pos, bps, bs, loop, mon); mon.newBlock(); blocks++;
            pos += bs * bps;
            if (loop && cur->total > 0 && std::floor(before / cur->total) != std::floor(pos / cur->total)) wraps++;
            if (mon.overlaps || mon.orphans || mon.order) { errors++; std::printf("scenario %d step %d: overlaps %ld orphans %ld out-of-order %ld\n", sc, step, mon.overlaps, mon.orphans, mon.order); break; }
        }
        player.allOff(0, mon);
        if (mon.sounding()) { stuckAfterStop++; errors++; std::printf("scenario %d: %d notes still on after the final stop\n", sc, mon.sounding()); }
        if (mon.ons != mon.offs) { errors++; std::printf("scenario %d: %ld note-ons, %ld note-offs\n", sc, mon.ons, mon.offs); }
        totalOns += mon.ons; totalOffs += mon.offs;
    }
    std::printf("scenarios: %d   blocks: %ld   note-ons: %ld   note-offs: %ld   stops: %ld   locates: %ld   loop wraps: %ld   timeline swaps: %ld\n", N, blocks, totalOns, totalOffs, stops, jumps, wraps, swaps);
    std::printf("straight runs: %ld with %ld notes; missing or extra note-ons: %ld; note-ons off by more than 1.5 samples: %ld\n", exactRuns, exactNotes, exactMissing, exactLate);
    std::printf("stuck notes after a stop: %ld   scenarios with any error: %ld\n", stuckAfterStop, errors);
    std::printf(errors || exactMissing || exactLate ? "PLAYER TEST FAILED\n" : "player test passed\n");
    return errors || exactMissing || exactLate ? 1 : 0;
}
