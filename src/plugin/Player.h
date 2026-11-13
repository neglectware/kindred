// Turns a timeline of notes into MIDI in step with a transport position.
// No JUCE and no allocation: the render thread calls process() once per block.
//
// The rule this class exists for: every note-on it sends is matched by an
// explicit note-off, whatever the transport does (stop, locate, loop, tempo
// change, a new timeline arriving in the middle of a chord).
#pragma once
#include "../engine/Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace kindred {

struct PNote {
    double on = 0, off = 0;       // beats from the start of the timeline
    uint8_t pitch = 60, vel = 72, ch = 0;
    bool bass = false;
};
struct TimeMark { double t = 0, len = 0; int sec = -1, slot = -1, block = -1; char kind = 'c'; };   // c chord, s suspension, t tail, l lead-in
struct Timeline {
    std::vector<PNote> notes;     // sorted by `on`
    std::vector<int> byOff;       // indexes into notes, sorted by `off`
    std::vector<TimeMark> marks;      // chord changes, for the display (not used by the render thread)
    double total = 0;             // length in beats
    uint64_t id = 0;              // changes whenever the content changes
};

struct PlayParams {
    int part = 0;                 // 0 both, 1 chords, 2 bass
    int chordCh = 0, bassCh = 1;  // 0..15
    int chordOct = 0, bassOct = 0;
    int chordVel = 72, bassVel = 80;
    double humanize = 0, strum = 0, gate = 1;
    bool operator==(const PlayParams& o) const {
        return part == o.part && chordCh == o.chordCh && bassCh == o.bassCh && chordOct == o.chordOct && bassOct == o.bassOct && chordVel == o.chordVel
            && bassVel == o.bassVel && humanize == o.humanize && strum == o.strum && gate == o.gate;
    }
};

// Builds the notes of a timeline from engine events (already tied or not).
inline void buildNotes(Timeline& tl, const std::vector<Event>& events, const PlayParams& pp) {
    auto hash = [](uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return (double) x / 4294967296.0; };
    struct Tmp { PNote n; int order; };
    std::vector<Tmp> out;
    int order = 0;
    for (auto& e : events) {
        if (e.bass ? pp.part == 1 : pp.part == 2) continue;
        for (int raw : e.notes) {
            const int pitch = raw + 12 * (e.bass ? pp.bassOct : pp.chordOct);
            if (pitch < 0 || pitch > 127 || !(e.len > 0)) continue;
            PNote n; n.on = e.t; n.off = e.t + e.len; n.pitch = (uint8_t) pitch; n.bass = e.bass; n.ch = (uint8_t) ((e.bass ? pp.bassCh : pp.chordCh) & 15);
            const uint32_t seed = (uint32_t) (order * 2654435761u) ^ (uint32_t) (pitch * 40503u) ^ (uint32_t) std::llround(e.t * 960.0);
            int vel = e.bass ? pp.bassVel : pp.chordVel;
            if (pp.humanize > 0) {
                vel += (int) std::lround((hash(seed) - .5) * 2 * 14 * pp.humanize);
                if (e.len >= .5) n.on += hash(seed ^ 0x9e3779b9u) * 0.05 * pp.humanize;       // up to 1/20 of a beat late, never early
            }
            n.vel = (uint8_t) std::max(1, std::min(127, vel));
            out.push_back({ n, order++ });
        }
    }
    // Strum: chord notes that start together are spread from the lowest note up.
    if (pp.strum > 0) {
        std::vector<Tmp*> grp;
        std::vector<Tmp*> all; for (auto& t : out) if (!t.n.bass) all.push_back(&t);
        std::stable_sort(all.begin(), all.end(), [](Tmp* a, Tmp* b) { return a->n.on != b->n.on ? a->n.on < b->n.on : a->n.pitch < b->n.pitch; });
        size_t i = 0;
        while (i < all.size()) {
            size_t j = i; while (j < all.size() && std::fabs(all[j]->n.on - all[i]->n.on) < 0.06) j++;
            const size_t count = j - i;
            if (count > 1) { const double step = 0.4 * pp.strum / (double) (count - 1);
                for (size_t k = i; k < j; k++) all[k]->n.on += step * (double) (k - i); }
            i = j;
        }
    }
    for (auto& t : out) {
        double len = t.n.off - t.n.on;
        if (pp.gate < 1) len *= std::max(.05, pp.gate);
        if (len < 1.0 / 64) len = 1.0 / 64;
        t.n.off = t.n.on + len;
    }
    std::stable_sort(out.begin(), out.end(), [](const Tmp& a, const Tmp& b) { return a.n.on < b.n.on; });
    tl.notes.clear(); tl.byOff.clear();
    for (auto& t : out) tl.notes.push_back(t.n);
    for (int i = 0; i < (int) tl.notes.size(); i++) tl.byOff.push_back(i);
    std::stable_sort(tl.byOff.begin(), tl.byOff.end(), [&](int a, int b) { return tl.notes[(size_t) a].off < tl.notes[(size_t) b].off; });
}

class Player {
public:
    Player() { reset(); }
    // Forgets everything without sending anything (after the host has reset its own state).
    void reset() { std::memset(count, 0, sizeof count); sounding = 0; running = false; cursor = 0; iOn = iOff = 0; lastId = 0; usedChannels = 0; }
    int soundingCount() const { return sounding; }
    // Bit set of the pitches that are sounding on any channel (for the keyboard display).
    void pitchMask(uint64_t& lo, uint64_t& hi) const {
        lo = hi = 0; if (!sounding) return;
        for (int ch = 0; ch < 16; ch++) for (int p = 0; p < 128; p++) if (count[ch][p]) { if (p < 64) lo |= 1ull << p; else hi |= 1ull << (p - 64); }
    }
    bool isRunning() const { return running; }

    // Ends every sounding note with its own note-off, then releases the sustain
    // pedal and sends All Notes Off on each channel that was used, as a backstop.
    template <class Emit> void allOff(int sample, Emit&& emit) {
        const bool any = sounding > 0;
        for (int ch = 0; ch < 16; ch++) for (int p = 0; p < 128; p++)
            if (count[ch][p]) { emit(sample, (uint8_t) (0x80 | ch), (uint8_t) p, (uint8_t) 0); count[ch][p] = 0; }
        sounding = 0;
        if (running || any) for (int ch = 0; ch < 16; ch++) if (usedChannels & (1u << ch)) {
            emit(sample, (uint8_t) (0xB0 | ch), (uint8_t) 64, (uint8_t) 0);
            emit(sample, (uint8_t) (0xB0 | ch), (uint8_t) 123, (uint8_t) 0);
        }
        usedChannels = 0; running = false;
    }

    // Renders one block. `pos` is the timeline position (beats) at sample 0,
    // `bps` is beats per sample. With `loop`, the timeline repeats.
    template <class Emit> void process(const Timeline* tl, double pos, double bps, int numSamples, bool loop, Emit&& emit) {
        if (!tl || tl->notes.empty() || !(tl->total > 0) || numSamples <= 0 || !(bps > 0)) { if (running || sounding) allOff(0, emit); if (tl) lastId = tl->id; return; }
        const double total = tl->total;
        if (loop) { pos = std::fmod(pos, total); if (pos < 0) pos += total; }
        else if (pos < 0) {       // before the start: silent until the timeline begins inside this block
            const double end = pos + bps * numSamples;
            if (end <= 0) { if (running || sounding) allOff(0, emit); lastId = tl->id; return; }
            if (running || sounding) allOff(0, emit);
            running = true; lastId = tl->id; iOn = iOff = 0; cursor = 0;
            scan(*tl, 0, end, pos, bps, numSamples, emit);
            return;
        }
        // A small difference from where the last block ended is clock noise (tempo ramps,
        // rounding in the host): keep going from our own cursor so nothing is sent twice.
        // A large one is a locate or a loop: settle the sounding notes on the new position.
        const double slack = std::max(0.02, 3 * bps * numSamples);
        const bool jump = !running || tl->id != lastId || std::fabs(pos - cursor) > slack;
        if (jump) resync(*tl, pos, emit);
        running = true; lastId = tl->id;
        double end = pos + bps * numSamples;
        if (loop && end >= total) {
            // Play to the end of the timeline, close everything at the wrap point, then start from the top.
            scan(*tl, cursor, total + 1e-9, pos, bps, numSamples, emit);
            const int wrapSample = std::max(0, std::min(numSamples - 1, (int) std::ceil((total - pos) / bps)));
            notesOff(wrapSample, emit);
            iOn = iOff = 0; cursor = 0;
            const double pos2 = pos - total;                     // negative: the top of the timeline is at wrapSample
            end -= total;
            if (end > total) end = total;                        // a block longer than the whole timeline: one pass only
            scan(*tl, 0, end, pos2, bps, numSamples, emit);
        } else {
            scan(*tl, cursor, end, pos, bps, numSamples, emit);
        }
    }

private:
    template <class Emit> void noteOn(const PNote& n, int sample, Emit&& emit) {
        uint8_t& c = count[n.ch][n.pitch];
        if (c) emit(sample, (uint8_t) (0x80 | n.ch), n.pitch, (uint8_t) 0);        // same pitch again: end the old one first
        else sounding++;
        emit(sample, (uint8_t) (0x90 | n.ch), n.pitch, n.vel);
        if (c < 255) c++;
        usedChannels |= 1u << n.ch;
    }
    template <class Emit> void noteOff(const PNote& n, int sample, Emit&& emit) {
        uint8_t& c = count[n.ch][n.pitch];
        if (!c) return;
        if (--c == 0) { emit(sample, (uint8_t) (0x80 | n.ch), n.pitch, (uint8_t) 0); sounding--; }
    }
    template <class Emit> void notesOff(int sample, Emit&& emit) {
        for (int ch = 0; ch < 16; ch++) for (int p = 0; p < 128; p++)
            if (count[ch][p]) { emit(sample, (uint8_t) (0x80 | ch), (uint8_t) p, (uint8_t) 0); count[ch][p] = 0; }
        sounding = 0;
    }
    // Sends the offs and ons that fall in [from, to), in time order, offs first at equal times.
    template <class Emit> void scan(const Timeline& tl, double from, double to, double posAtSample0, double bps, int numSamples, Emit&& emit) {
        const size_t n = tl.notes.size();
        (void) from;
        while (true) {
            const double tOff = iOff < n ? tl.notes[(size_t) tl.byOff[iOff]].off : 1e300;
            const double tOn = iOn < n ? tl.notes[iOn].on : 1e300;
            const double t = std::min(tOff, tOn);
            if (t >= to) break;
            const int sample = std::max(0, std::min(numSamples - 1, (int) std::floor((t - posAtSample0) / bps + 0.5)));
            if (tOff <= tOn) { noteOff(tl.notes[(size_t) tl.byOff[iOff]], sample, emit); iOff++; }
            else { noteOn(tl.notes[iOn], sample, emit); iOn++; }
        }
        cursor = to;
    }
    // Makes the sounding notes equal to the notes that are in progress at `pos`:
    // ends what should not sound, starts what should, and leaves the rest alone.
    template <class Emit> void resync(const Timeline& tl, double pos, Emit&& emit) {
        uint8_t want[16][128]; uint8_t vel[16][128];
        std::memset(want, 0, sizeof want);
        const size_t n = tl.notes.size();
        size_t on = 0;
        { size_t lo = 0, hi = n; while (lo < hi) { const size_t mid = (lo + hi) / 2; if (tl.notes[mid].on < pos) lo = mid + 1; else hi = mid; } on = lo; }
        for (size_t i = 0; i < on; i++) { const PNote& x = tl.notes[i]; if (x.off > pos) { if (want[x.ch][x.pitch] < 255) want[x.ch][x.pitch]++; vel[x.ch][x.pitch] = x.vel; } }
        for (int ch = 0; ch < 16; ch++) for (int p = 0; p < 128; p++) {
            const uint8_t have = count[ch][p], w = want[ch][p];
            if (have && !w) { emit(0, (uint8_t) (0x80 | ch), (uint8_t) p, (uint8_t) 0); sounding--; }
            else if (!have && w) { emit(0, (uint8_t) (0x90 | ch), (uint8_t) p, vel[ch][p]); sounding++; usedChannels |= 1u << ch; }
            count[ch][p] = w;
        }
        iOn = on;
        { size_t lo = 0, hi = n; while (lo < hi) { const size_t mid = (lo + hi) / 2; if (tl.notes[(size_t) tl.byOff[mid]].off <= pos) lo = mid + 1; else hi = mid; } iOff = lo; }
        cursor = pos;
    }

    uint8_t count[16][128];
    int sounding = 0;
    bool running = false;
    double cursor = 0;
    size_t iOn = 0, iOff = 0;
    uint64_t lastId = 0;
    uint32_t usedChannels = 0;
};

}  // namespace kindred
