// Port of the prototype's engine.js. The order of every random draw and of every
// floating-point operation follows the original, so the same seed gives the same
// music (see tests/golden). Build this file with -ffp-contract=off.
#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <cstdlib>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kindred {

double Rng::next() {
    a += 0x6D2B79F5u;
    uint32_t t = (a ^ (a >> 15)) * (1u | a);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (double) (t ^ (t >> 14)) / 4294967296.0;
}

// Returns the chosen index, or -1 when there is nothing to choose from.
static int pickWeighted(const std::vector<double>& weights, Rng& rand) {
    const int n = (int) weights.size();
    double sum = 0;
    for (double w : weights) sum = sum + std::max(0.0, w);
    if (sum <= 0) { double r = rand.next(); return n ? (int) std::floor(r * n) : -1; }
    double r = rand.next() * sum;
    for (int i = 0; i < n; i++) { r -= std::max(0.0, weights[(size_t) i]); if (r <= 0) return i; }
    return n - 1;
}

static const std::vector<int>& triad(const std::string& q) {
    static const std::vector<int> maj{0,4,7}, min{0,3,7}, dim{0,3,6}, aug{0,4,8};
    return q == "min" ? min : q == "dim" ? dim : q == "aug" ? aug : maj;
}
static bool isTriadQuality(const std::string& q) { return q == "maj" || q == "min" || q == "dim" || q == "aug"; }
static const char* triadQuality(int r, int t, int f) {
    const int a = mod12(t - r), b = mod12(f - r);
    if (a == 4 && b == 7) return "maj";
    if (a == 3 && b == 7) return "min";
    if (a == 3 && b == 6) return "dim";
    if (a == 4 && b == 8) return "aug";
    return "";
}
static std::vector<Cand> diatonicTriads(const std::vector<int>& steps) {
    std::vector<Cand> out;
    for (size_t d = 0; d < 7; d++) { Cand c; c.deg = steps[d]; c.q = triadQuality(steps[d], steps[(d + 2) % 7], steps[(d + 4) % 7]); out.push_back(c); }
    return out;
}
static unsigned bit(int pc) { return 1u << mod12(pc); }
static unsigned triadSet(const Cand& c) { unsigned s = 0; for (int i : triad(c.q)) s |= bit(c.deg + i); return s; }
static int commonTones(const Cand& a, const Cand& b) { unsigned s = triadSet(a) & triadSet(b); int n = 0; while (s) { n += (int) (s & 1); s >>= 1; } return n; }
static bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
static int degOf(const Project& p, const Chord& ch) { return mod12(ch.root - p.key); }
static const int kFifthDeg = 7;

double* Mood::byId(const std::string& id) {
    if (id == "wistful") return &wistful; if (id == "moving") return &moving; if (id == "color") return &color;
    if (id == "inner") return &inner; if (id == "surprise") return &surprise; if (id == "leadins") return &leadins;
    if (id == "edge") return &edge;
    return nullptr;
}
Section* Project::section(const std::string& id) { for (auto& s : sections) if (s.id == id) return &s; return nullptr; }

// ------------------------------------------------------------------ theory ----
bool Engine::isDiatonic(const std::string& mode, const Cand& c) const {
    const ModeDef* m = st->mode(mode); if (!m) return false;
    for (auto& t : diatonicTriads(m->steps)) if (t.deg == c.deg && t.q == c.q) return true;
    return false;
}
const std::vector<int>* Engine::parentScale(const std::string& mode, const Cand& c) const {
    if (isDiatonic(mode, c)) return &st->modeOr(mode).steps;
    for (auto& m : st->modes) if (isDiatonic(m.id, c)) return &m.steps;
    return nullptr;
}
Cand Engine::asCand(const Project& p, const Chord& ch) {
    Cand c; c.deg = degOf(p, ch); c.q = isTriadQuality(ch.q) ? ch.q : "maj"; c.kind = ch.kind.empty() ? "diatonic" : ch.kind; return c;
}
unsigned Engine::chordPcs(const Chord& ch) {
    unsigned s = 0; for (int i : ch.iv) s |= bit(ch.root + i);
    if (ch.bass) s |= bit(*ch.bass);
    return s;
}
static unsigned relPcs(const Project& p, const Chord& ch) {      // chordPcs relative to the key
    unsigned s = 0; for (int i : ch.iv) s |= bit(ch.root + i - p.key);
    if (ch.bass) s |= bit(*ch.bass - p.key);
    return s;
}
// A chord over a drone it does not contain the root of. Some chords take their
// own root instead (the drone lets go for that chord); the style lists them.
bool Engine::pedalOk(const Cand& c, char bt) const {
    if (bt == 'T') return !contains(st->tonicDroneExclude, c.deg);
    if (bt == 'F') return contains(st->fifthDroneAllow, c.deg);
    return true;
}
int Engine::bassDeg(const Cand& c, char bt) const {
    return bt == 'T' && pedalOk(c, bt) ? 0 : bt == 'F' && pedalOk(c, bt) ? kFifthDeg : c.deg;
}
double Engine::tension(const Cand& c, int bdeg) const {
    auto it = st->tension.find(c.deg);
    const double t = it != st->tension.end() ? it->second : st->tensionDefault;
    if (bdeg == c.deg) return t;
    return bdeg == 0 ? t * 0.8 : std::max(.6, t + .2);
}
std::vector<Cand> Engine::vocab(const std::string& mode) const {
    std::vector<Cand> out;
    for (auto& t : diatonicTriads(st->modeOr(mode).steps)) if (t.q == "maj" || t.q == "min") out.push_back(t);
    return out;
}
char Engine::functionOf(const Project& p, const Chord& ch) {
    if (ch.kind == "borrowed") return 'B';
    if (ch.kind == "approach") return 'A';
    switch (degOf(p, ch)) {
        case 0: return 'T';
        case 5: case 2: return 'S';
        case 7: case 11: case 10: return 'D';
        default: return 'M';
    }
}

// ------------------------------------------------------------------- names ----
static const char* kSharp[12] = {"C","C♯","D","D♯","E","F","F♯","G","G♯","A","A♯","B"};
static const char* kFlat[12]  = {"C","D♭","D","E♭","E","F","G♭","G","A♭","A","B♭","B"};
static const char* kRoman[12] = {"I","♭II","II","♭III","III","IV","♯IV","V","♭VI","VI","♭VII","VII"};
static bool flatKey(int k) { return k == 5 || k == 10 || k == 3 || k == 8 || k == 1 || k == 6; }
std::string Engine::noteName(int pc, int keyPc, int deg, bool preferSharp) {
    const bool flat = !preferSharp && (flatKey(keyPc) || deg == 1 || deg == 3 || deg == 8 || deg == 10);
    return (flat ? kFlat : kSharp)[mod12(pc)];
}
std::string Engine::chordName(const Chord& ch, int keyPc) {
    std::string name = noteName(ch.root, keyPc, mod12(ch.root - keyPc), ch.sharp) + ch.ext;
    if (ch.bass && mod12(*ch.bass) != ch.root) return name + "/" + noteName(mod12(*ch.bass), keyPc, mod12(*ch.bass - keyPc), false);
    return name;
}
static std::string lowerAscii(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a'); return s; }
std::string Engine::romanName(const Chord& ch, int keyPc) {
    std::string n = kRoman[mod12(ch.root - keyPc)];
    if (ch.q == "min" || ch.q == "dim") n = lowerAscii(n);
    std::string s = ch.ext;
    if (ch.q == "min" && !s.empty() && s[0] == 'm') s = s.substr(1);
    if (ch.bass && mod12(*ch.bass) != ch.root) s += std::string("/") + kRoman[mod12(*ch.bass - keyPc)];
    return n + s;
}
std::string Engine::keyName(int k) { return (flatKey(mod12(k)) ? kFlat : kSharp)[mod12(k)]; }
std::string Engine::pcName(int m, int keyPc) { return noteName(mod12(m), keyPc, mod12(m - keyPc), false); }

// ---------------------------------------------------------------- skeleton ----
const Section* Engine::homeSection(const Project& p, const Section* sec) const {
    for (auto& s : p.sections) {
        if (&s == sec || s.role != "home") continue;
        for (auto& x : s.slots) if (x.chord) return &s;
    }
    return nullptr;
}
std::vector<std::string> Engine::allowedPlans(const Project& p, const std::string& role) const {
    const RoleDef* r = st->role(role); if (!r) r = st->role("home");
    const bool fifthOk = !st->cellList(p.mode, "ped5").empty();
    std::vector<std::string> out;
    for (auto& id : r->plans) {
        const PlanDef* pl = st->plan(id);
        const bool usesFifth = pl && (pl->a == 'F' || (pl->kind != PlanDef::All && pl->b == 'F'));
        if (usesFifth && !fifthOk) continue;
        out.push_back(id);
    }
    return out;
}
std::string Engine::choosePlan(const Project& p, const Section& sec, Rng& rand) const {
    const auto ok = allowedPlans(p, sec.role);
    if (!sec.planPref.empty() && sec.planPref != "auto" && std::find(ok.begin(), ok.end(), sec.planPref) != ok.end()) return sec.planPref;
    const double M = p.mood.moving, D = 1 - M;
    const RoleDef* role = st->role(sec.role);
    const Section* home = homeSection(p, &sec);
    const ModeDef& mode = st->modeOr(p.mode);
    std::vector<double> w;
    for (auto& id : ok) {
        double x = .2;
        if (role) { auto it = role->planWeights.find(id);
            if (it != role->planWeights.end()) { x = 0; for (auto& t : it->second) { double v = t.c; for (int k = 0; k < t.powD; k++) v = v * D; for (int k = 0; k < t.powM; k++) v = v * M; x = x + v; } } }
        if (home && home->plan && home->plan->id == id && sec.role != "return") x *= st->rules.homePlanRepeat;
        const PlanDef* pl = st->plan(id);
        if (pl && pl->kind == PlanDef::All && pl->a == 'F') x *= 1.5 - p.mood.wistful;
        if (!mode.planBias.empty()) {            // for example: Lydian needs its tonic underneath
            auto it = mode.planBias.find(id); if (it == mode.planBias.end()) it = mode.planBias.find("*");
            if (it != mode.planBias.end()) x *= it->second;
        }
        w.push_back(x);
    }
    const int k = pickWeighted(w, rand);
    return k >= 0 ? ok[(size_t) k] : st->plans.front().id;
}
double Engine::cellWeight(const Project& p, const CellDef& cell, const std::vector<char>& types, int off, const std::vector<std::optional<Cand>>& locked) const {
    const Mood& m = p.mood; const Rules& R = st->rules;
    double w = cell.w * std::max(R.cellMinWeight, 1 + (m.moving - .5) * R.cellDriveSlope * (cell.d - R.cellDrivePivot));
    if (!st->modeOr(p.mode).minor) {
        int minors = 0; for (auto& c : cell.chords) if (c.q == "min") minors++;
        w *= std::pow(R.minorBase + R.minorSlope * m.wistful, minors);
    }
    for (size_t k = 0; k < cell.chords.size(); k++) {
        const Cand& c = cell.chords[k];
        const size_t at = (size_t) off + k;
        const char bt = at < types.size() ? types[at] : 'R';
        if (bt != 'R' && !pedalOk(c, bt)) w *= R.pedalMisfit;
        if (at < locked.size() && locked[at]) w *= ckey(*locked[at]) == ckey(c) ? R.lockedMatch : R.lockedMiss;
    }
    return w;
}
// The chords of a section before any color. `locked` holds the chords of locked slots.
Engine::Skeleton Engine::chooseSkeleton(const Project& p, const Section& sec, int n, const std::string& planId,
                                        const std::vector<std::optional<Cand>>& locked, Rng& rand) const {
    Skeleton sk;
    const PlanDef* plan = st->plan(planId);
    sk.types = plan ? plan->types(n) : std::vector<char>((size_t) n, 'R');
    const std::string& mode = p.mode, role = sec.role;
    const Section* home = homeSection(p, &sec);
    std::vector<std::string> homeSet; bool hasHome = home != nullptr;
    if (home) for (auto& x : home->slots) if (x.chord) homeSet.push_back(ckey(asCand(p, *x.chord)));
    auto inHome = [&](const std::string& k) { return std::find(homeSet.begin(), homeSet.end(), k) != homeSet.end(); };
    using Extra = std::function<double(const CellDef&)>;
    auto pick = [&](const std::vector<CellDef>& cells, int off, const Extra& extra) -> const CellDef* {
        std::vector<double> w;
        for (auto& c : cells) w.push_back(cellWeight(p, c, sk.types, off, locked) * (extra ? extra(c) : 1));
        const int k = pickWeighted(w, rand);
        return k >= 0 ? &cells[(size_t) k] : nullptr;
    };
    Extra related = [&](const CellDef& c) -> double {
        if (!hasHome) return 1;
        std::vector<std::string> seen;
        for (auto& ch : c.chords) { auto k = ckey(ch); if (inHome(k) && std::find(seen.begin(), seen.end(), k) == seen.end()) seen.push_back(k); }
        return .3 + (double) seen.size();
    };
    // How closely a second half follows the first: same first two chords, same first chord, or neither.
    auto near = [&](const CellDef& c, const CellDef& a) -> double {
        return ckey(c.chords[0]) != ckey(a.chords[0]) ? (role == "home" ? 0 : .12) : ckey(c.chords[1]) == ckey(a.chords[1]) ? 1 : .3;
    };
    auto rocking = [](const CellDef& c) { return ckey(c.chords[0]) == ckey(c.chords[2]) && ckey(c.chords[1]) == ckey(c.chords[3]); };
    auto open = [](const CellDef& c) { return c.chords[3].deg != 0; };
    const bool overFifth = plan && plan->kind == PlanDef::All && plan->a == 'F';
    const std::string kind = role == "home" ? "home" : role == "away" ? (overFifth ? "ped5" : "away") : role == "return" ? "close" : "lift";
    std::vector<CellDef> cells = st->cellList(mode, kind);
    if (cells.empty()) cells = st->cellList(mode, "home");
    if (role == "turn") {
        std::vector<CellDef> o; for (auto& c : cells) if (open(c)) o.push_back(c);
        if (o.empty()) for (auto& c : st->cellList(mode, "away")) if (open(c)) o.push_back(c);
        if (!o.empty()) cells = o;
    }
    auto diatonic = [](Cand c) { c.kind = "diatonic"; return c; };
    if (role == "return") {
        const CellDef* b = pick(cells, n - 4, related);
        std::vector<Cand> a;
        if (n > 4) {
            std::vector<Cand> hs;
            if (home) for (size_t i = 0; i < home->slots.size() && i < 4; i++) if (home->slots[i].chord) hs.push_back(asCand(p, *home->slots[i].chord));
            if (hs.size() == 4) { a = hs; sk.from = home->name; }       // keeps each chord's own kind
            else { const CellDef* ac = pick(st->cellList(mode, "home"), 0, nullptr); if (ac) for (auto& c : ac->chords) a.push_back(diatonic(c)); }
        }
        sk.seq = a;
        if (b) for (auto& c : b->chords) sk.seq.push_back(diatonic(c));
        return sk;
    }
    const CellDef* a = pick(cells, 0, role == "home" ? Extra() : related);
    const CellDef* b = nullptr;
    if (n > 4 && a) {
        b = pick(cells, 4, [&](const CellDef& c) -> double {
            double x = near(c, *a) * (c.id == a->id ? (c.chords[3].deg == 0 ? .25 : rocking(c) && role != "home" ? .25 : .6) : 1);
            if (role == "lift" || role == "turn") x *= open(c) ? 1.3 : .4;       // a chorus hands back to the verse
            if (role == "away") x *= open(c) ? 1.5 : .5;
            return x;
        });
    }
    if (home && role != "home") sk.from = home->name;
    if (a) for (auto& c : a->chords) sk.seq.push_back(diatonic(c));
    if (b) for (auto& c : b->chords) sk.seq.push_back(diatonic(c));
    return sk;
}
// At most one borrowed chord per section: the minor iv before I, or bVII in
// place of V or ii before I or IV. With the edge control up, the style's edge
// chords join the list and a second borrowed chord is allowed.
void Engine::borrowOne(const Project& p, const Section& sec, std::vector<Cand>& seq, const std::vector<char>& types,
                       const std::function<bool(int)>& isFixed, Rng& rand) const {
    const Mood& m = p.mood; const int n = (int) seq.size(); const Rules& R = st->rules;
    const ModeDef& mode = st->modeOr(p.mode);
    struct Opt { int j; Cand c; double w; };
    auto options = [&](bool edge) {
        std::vector<Opt> opts;
        for (int j = sec.role == "return" ? std::max(1, n - 4) : 1; j < n; j++) {
            if (isFixed(j) || types[(size_t) j] == 'F') continue;
            const Cand& c = seq[(size_t) j]; const Cand& pv = seq[(size_t) j - 1];
            const Cand* nx = j + 1 < n ? &seq[(size_t) j + 1] : (sec.role == "return" ? nullptr : &seq[0]);
            if (!nx) continue;
            if (!edge) {
                if (mode.borrowMinorFour && c.deg == 5 && c.q == "maj" && nx->deg == 0 && pv.deg != 5)
                    opts.push_back({ j, Cand{5, "min", "borrowed"}, (.3 + m.wistful) * (.2 + (double) j / n) });      // best late in the phrase
                if (mode.borrowFlatSeven && ((c.deg == 7 && c.q == "maj") || c.deg == 2) && (nx->deg == 0 || nx->deg == 5) && pv.deg != 10)
                    opts.push_back({ j, Cand{10, "maj", "borrowed"}, 1 });
            } else {
                if (c.kind == "borrowed" || c.deg == 0) continue;
                for (auto& b : st->edgeBorrow) {
                    Cand bc{ b.deg, b.q, "borrowed" };
                    if (isDiatonic(p.mode, bc) || (pv.deg == b.deg) || (nx->deg == b.deg)) continue;
                    // A borrowed chord must still touch a neighbor: it shares a triad tone with the chord before or after.
                    if (commonTones(bc, pv) + commonTones(bc, *nx) == 0) continue;
                    opts.push_back({ j, bc, 1.0 + commonTones(bc, pv) + commonTones(bc, *nx) });
                }
            }
        }
        return opts;
    };
    bool any = false; for (auto& c : seq) if (c.kind == "borrowed") any = true;
    if ((mode.borrowMinorFour || mode.borrowFlatSeven) && !any && !(rand.next() >= m.surprise * (sec.role == "away" ? R.borrowChanceAway : R.borrowChance))) {
        auto opts = options(false);
        if (!opts.empty()) { std::vector<double> w; for (auto& o : opts) w.push_back(o.w); const auto& o = opts[(size_t) pickWeighted(w, rand)]; seq[(size_t) o.j] = o.c; }
    }
    if (m.edge > 0 && !st->edgeBorrow.empty()) {
        int have = 0; for (auto& c : seq) if (c.kind == "borrowed") have++;
        for (int k = have; k < R.edgeMaxBorrowed; k++) {
            if (rand.next() >= m.edge * R.edgeBorrowChance) break;
            auto opts = options(true);
            if (opts.empty()) break;
            std::vector<double> w; for (auto& o : opts) w.push_back(o.w);
            const auto& o = opts[(size_t) pickWeighted(w, rand)]; seq[(size_t) o.j] = o.c;
        }
    }
}

// ------------------------------------------------------------------- color ----
std::vector<const ExtDef*> Engine::palette(const Project& p, const Cand& c, bool foreign, int bdeg) const {
    std::vector<const ExtDef*> out;
    auto it = st->extensions.find(c.q); if (it == st->extensions.end()) return out;
    const std::vector<int>* scale = parentScale(p.mode, c);
    const auto& tri = triad(c.q);
    for (auto& e : it->second) {
        if (c.kind == "borrowed" && e.t != "plain" && !(e.t == "add9" && c.deg == 10)) continue;
        if (e.t == "jazz" && ((!p.extended && !(p.mood.edge > 0)) || foreign)) continue;
        if (scale) { bool all = true; for (int i : e.iv) if (!contains(*scale, mod12(c.deg + i))) all = false; if (!all) continue; }
        if (e.t == "sus4") { if (c.deg == 7 && !foreign) out.push_back(&e); continue; }
        // Over a drone: plain, add9, or maj7 on IV, and the added note is never a sixth above the bass.
        if (foreign) {
            bool sixth = false; for (int i : e.iv) if (!contains(tri, i) && mod12(c.deg + i - bdeg) == 9) sixth = true;
            if (e.t == "plain" || ((e.t == "add9" || (e.t == "maj7" && c.deg == 5)) && !sixth)) out.push_back(&e);
            continue;
        }
        out.push_back(&e);
    }
    return out;
}
double Engine::typeWeight(const ExtDef& e, const Cand& c, double r, bool foreign) const {
    auto it = st->colorWeights.find(e.t);
    const ColorWeight& cw = it != st->colorWeights.end() ? it->second : st->colorWeights.at("default");
    double x = cw.c0 + cw.cr * r + cw.cinv * (1 - r);
    double df = cw.degDefault;
    auto d = cw.deg.find(ckey(c)); if (d == cw.deg.end()) d = cw.deg.find(std::to_string(c.deg));
    if (d != cw.deg.end()) df = d->second;
    if (cw.degOwnRootOnly && foreign) df = cw.degDefault;
    x = x * df;
    if (foreign) x = x * cw.drone;
    return x;
}
static unsigned extPcs(const Cand& c, const ExtDef& e, int bdeg) { unsigned s = bit(bdeg); for (int i : e.iv) s |= bit(c.deg + i); return s; }
// Chooses one color per distinct chord in the section, favoring an added note
// that a neighbor also has, then makes sure every change shares a note.
// `given[i]` is the set of sounding pitch classes (relative to the key) of a
// slot that must not change; such slots get no entry in the result.
Engine::Colored Engine::colorize(const Project& p, const std::vector<Cand>& seq, const std::vector<int>& bdegs,
                                 const std::vector<std::optional<unsigned>>& given, bool wrap, Rng& rand, bool restLast) const {
    const int n = (int) seq.size(); const double r = p.mood.color; const Rules& R = st->rules;
    Colored res; auto& exts = res.exts; exts.assign((size_t) n, nullptr);
    auto foreign = [&](int i) { return bdegs[(size_t) i] != seq[(size_t) i].deg; };
    auto baseSet = [&](int i) -> unsigned { return given[(size_t) i] ? *given[(size_t) i] : (triadSet(seq[(size_t) i]) | bit(bdegs[(size_t) i])); };
    std::vector<std::pair<std::string, std::vector<int>>> groups;
    for (int i = 0; i < n; i++) {
        if (given[(size_t) i]) continue;
        const std::string k = ckey(seq[(size_t) i]) + (foreign(i) ? "/" + std::to_string(bdegs[(size_t) i]) : "");
        auto it = std::find_if(groups.begin(), groups.end(), [&](auto& g) { return g.first == k; });
        if (it == groups.end()) groups.push_back({ k, { i } }); else it->second.push_back(i);
    }
    const bool edgy = p.mood.edge > 0 && !p.extended;
    for (auto& g : groups) {
        const auto& idxs = g.second;
        const Cand& c = seq[(size_t) idxs[0]]; const bool fo = foreign(idxs[0]);
        const auto pal = palette(p, c, fo, bdegs[(size_t) idxs[0]]);
        const unsigned tri = triadSet(c);
        std::vector<double> w;
        for (auto* e : pal) {
            double x = typeWeight(*e, c, r, fo);
            if (edgy && e->t == "jazz") x = R.edgeColorWeight * p.mood.edge;
            unsigned colors = 0; int ncolors = 0;
            for (int i : e->iv) { const unsigned b = bit(c.deg + i); if (!(tri & b)) { colors |= b; ncolors++; } }
            int conn = 0;
            for (int i : idxs) for (int j : { i - 1, i + 1 }) { if (j < 0 || j >= n) continue; if (colors & baseSet(j)) conn++; }
            if (ncolors) x *= 1 + R.connectBoost * conn / (2 * (double) idxs.size());       // an added note that a neighbor also has
            w.push_back(x);
        }
        const int k = pickWeighted(w, rand);
        for (int i : idxs) exts[(size_t) i] = k >= 0 ? pal[(size_t) k] : nullptr;
    }
    // The very last chord of a coda rests: a plain triad or add9.
    const int L = n - 1;
    if (restLast && n > 0 && !given[(size_t) L] && exts[(size_t) L] && exts[(size_t) L]->t != "plain" && exts[(size_t) L]->t != "add9") {
        std::vector<const ExtDef*> pal; for (auto* e : palette(p, seq[(size_t) L], false, 0)) if (e->t == "plain" || e->t == "add9") pal.push_back(e);
        std::vector<double> w; for (auto* e : pal) w.push_back(typeWeight(*e, seq[(size_t) L], r, false));
        const int k = pickWeighted(w, rand); if (k >= 0) exts[(size_t) L] = pal[(size_t) k];
    }
    // The same chord twice in a row: one of the two changes color, so one voice moves.
    for (int i = 1; i < n; i++) {
        if (given[(size_t) i] || given[(size_t) i - 1] || ckey(seq[(size_t) i]) != ckey(seq[(size_t) i - 1]) || bdegs[(size_t) i] != bdegs[(size_t) i - 1]
            || exts[(size_t) i] != exts[(size_t) i - 1]) continue;
        const int k = restLast && i == L ? i - 1 : i;
        std::vector<const ExtDef*> pal;
        for (auto* e : palette(p, seq[(size_t) k], foreign(k), bdegs[(size_t) k]))
            if (e != exts[(size_t) i] && e->t != "jazz" && !(k > 0 && k < i && ckey(seq[(size_t) k - 1]) == ckey(seq[(size_t) k]) && e == exts[(size_t) k - 1])) pal.push_back(e);
        if (pal.empty() || r <= 0) continue;
        std::vector<double> w; for (auto* e : pal) w.push_back(.2 + typeWeight(*e, seq[(size_t) k], std::max(r, .3), foreign(k)));
        exts[(size_t) k] = pal[(size_t) pickWeighted(w, rand)];
    }
    auto sound = [&](int i) -> unsigned { return given[(size_t) i] ? *given[(size_t) i] : (exts[(size_t) i] ? extPcs(seq[(size_t) i], *exts[(size_t) i], bdegs[(size_t) i]) : 0u); };
    auto fix = [&](int i, int j) -> bool {          // make slot i share a note with slot j
        if (given[(size_t) i] || (restLast && i == L)) return false;
        const unsigned S = sound(j); int other = i > j ? i + 1 : i - 1;
        if (R.seamKeepsNeighbor && n > 2) { if (i == n - 1 && j == 0) other = n - 2; else if (i == 0 && j == n - 1) other = 1; }   // across the loop seam
        const bool useKeep = other >= 0 && other < n && (given[(size_t) other] || (sound(i) & sound(other)));
        const unsigned keep = useKeep ? sound(other) : 0;
        std::vector<const ExtDef*> pal;
        for (auto* e : palette(p, seq[(size_t) i], foreign(i), bdegs[(size_t) i])) { const unsigned P = extPcs(seq[(size_t) i], *e, bdegs[(size_t) i]); if ((P & S) && (!useKeep || (P & keep))) pal.push_back(e); }
        if (pal.empty()) return false;
        std::vector<double> w; for (auto* e : pal) w.push_back(.1 + typeWeight(*e, seq[(size_t) i], .6, foreign(i)));
        exts[(size_t) i] = pal[(size_t) pickWeighted(w, rand)];
        return true;
    };
    // With the edge control up, some changes are allowed to share nothing.
    auto mayDisconnect = [&]() { return p.mood.edge > 0 && rand.next() < p.mood.edge * R.edgeDisconnect; };
    for (int i = 0; i + 1 < n; i++)
        if (!(sound(i) & sound(i + 1)) && !mayDisconnect() && !fix(i + 1, i) && !fix(i, i + 1)) res.ok = false;
    if (wrap && n > 1 && !(sound(n - 1) & sound(0)) && !mayDisconnect()) { if (!fix(n - 1, 0)) fix(0, n - 1); }
    return res;
}

// ----------------------------------------------------------------- voicing ----
// Upper voices inside the style's range, moving as little as possible; a note
// two chords share stays where it is.
std::vector<int> Engine::voice(const Chord& ch, const std::vector<int>* prevNotes) const {
    const Rules& R = st->rules;
    std::vector<int> pcs;
    for (int i : ch.iv) { const int pc = mod12(ch.root + i); if (!contains(pcs, pc)) pcs.push_back(pc); }
    std::vector<int> upper = pcs;
    auto drop = [&](int pc) { upper.erase(std::remove(upper.begin(), upper.end(), pc), upper.end()); };
    if (upper.size() >= 6) { drop(ch.root); drop(mod12(ch.root + 7)); }
    else if (upper.size() == 5) drop(ch.root);
    if (upper.size() == 3) upper.push_back(ch.root);
    const int LO = R.voiceLo, HI = R.voiceHi;
    std::vector<std::vector<int>> options;
    for (int pc : upper) { std::vector<int> a; for (int m = LO; m <= HI; m++) if (mod12(m) == pc) a.push_back(m); options.push_back(a); }
    const size_t N = upper.size();
    const bool hasPrev = prevNotes && !prevNotes->empty();
    auto nearest = [](int x, const std::vector<int>& arr) { int best = std::numeric_limits<int>::max(); for (int y : arr) best = std::min(best, std::abs(x - y)); return best; };
    for (int minGap : { 2, 1 }) {
        std::vector<int> best; bool found = false; double bestCost = std::numeric_limits<double>::infinity();
        std::vector<int> combo(N), s;
        std::function<void(size_t)> rec = [&](size_t k) {
            if (k == N) {
                s = combo; std::sort(s.begin(), s.end());
                for (size_t i = 1; i < s.size(); i++) { const int g = s[i] - s[i - 1]; if (g < minGap || (s[i] < R.voiceLowLimit && g < R.voiceLowMinGap)) return; }
                if (s.back() - s.front() > R.voiceMaxSpan) return;
                double sum = 0; for (int x : s) sum = sum + x;
                const double mean = sum / (double) s.size();
                double cost;
                if (hasPrev) {
                    const auto& pv = *prevNotes;
                    double a = 0, b = 0; for (int x : s) a = a + nearest(x, pv); for (int y : pv) b = b + nearest(y, s);
                    cost = (a + b) / 2;
                    const int leap = std::abs(s.back() - pv.back());
                    cost += 0.9 * leap + (leap > 4 ? 2 : 0) + 0.12 * std::fabs(mean - R.voiceCenter);
                } else {
                    cost = std::fabs(mean - R.voiceCenter);
                    for (size_t i = 1; i < s.size(); i++) if (s[i] - s[i - 1] < 3) cost += 0.5;
                }
                for (size_t i = 2; i < s.size(); i++) if (s[i] - s[i - 2] <= 4) cost += 3;      // three notes inside a third
                if (cost < bestCost) { bestCost = cost; best = s; found = true; }
                return;
            }
            for (int m : options[k]) { combo[k] = m; rec(k + 1); }
        };
        rec(0);
        if (found) return best;
    }
    std::vector<int> fb; for (int pc : upper) fb.push_back(60 + pc);
    std::sort(fb.begin(), fb.end());
    return fb;
}
// Bass notes take the nearest octave to the previous bass, inside the style's bass range.
int Engine::bassNote(int pc, int prev) const {
    int best = -1;
    for (int m = st->rules.bassLo; m <= st->rules.bassHi; m++)
        if (mod12(m) == pc && (best < 0 || std::abs(m - prev) < std::abs(best - prev))) best = m;
    return best;
}

// ---------------------------------------------------------- inner movement ----
// Suspension: one upper note of the previous chord is held over the change,
// then steps down into the new chord (4-3, 9-8, or b6-5 on a minor chord).
static std::optional<Inner> findSuspension(const std::vector<int>& prev, const std::vector<int>& cur, const Chord& ch, double beats) {
    unsigned pcs = 0; for (int i : ch.iv) pcs |= bit(ch.root + i);
    if (prev.empty()) return std::nullopt;
    const int top = *std::max_element(prev.begin(), prev.end()), third = mod12(ch.root + (ch.q == "min" ? 3 : 4));
    bool have = false; int bp = 0, bn = 0; double bscore = 0;
    for (int pn : prev) {
        if ((pcs & bit(pn)) || contains(cur, pn)) continue;
        const int iv = mod12(pn - ch.root);
        if (!(iv == 5 || iv == 2 || (ch.q == "min" && iv == 8))) continue;      // 4-3, 9-8, b6-5; never a held sixth
        if (ch.bass && mod12(pn - *ch.bass) == 1) continue;
        for (int n : cur) {
            const int d = pn - n;
            if (d != 1 && d != 2) continue;
            bool clash = false; for (int x : cur) if (x != n && (std::abs(x - pn) == 1 || std::abs(x - pn) == 13)) clash = true;
            if (clash) continue;
            const double score = 1 + (mod12(n) == third ? 1.2 : 0) + (pn == top ? .6 : 0) + (d == 1 ? .3 : 0);
            if (!have || score > bscore) { have = true; bp = pn; bn = n; bscore = score; }
        }
    }
    if (!have) return std::nullopt;
    Inner s; s.beats = beats; s.from = bp; s.to = bn;
    for (int x : cur) s.notes.push_back(x == bn ? bp : x);
    std::sort(s.notes.begin(), s.notes.end());
    return s;
}
// Tail: in the last part of a chord, one voice that is about to move a third
// passes through the scale note in between; the other voices hold.
static std::optional<Inner> findTail(const std::vector<int>& cur, const std::vector<int>& next, const Chord& ch, const std::vector<int>& scale, double beats) {
    if (cur.size() != next.size()) return std::nullopt;
    unsigned pcs = 0; for (int i : ch.iv) pcs |= bit(ch.root + i);
    bool have = false; int bX = 0, bP = 0; double bscore = 0;
    for (size_t k = 0; k < cur.size(); k++) {
        const int X = cur[k], Y = next[k], d = Y - X;
        if (std::abs(d) != 3 && std::abs(d) != 4) continue;
        for (int P : { d > 0 ? X + 1 : X - 1, d > 0 ? X + 2 : X - 2 }) {
            if (P == Y || std::abs(Y - P) > 2 || !contains(scale, mod12(P)) || (pcs & bit(P))) continue;
            const int iv = mod12(P - ch.root);
            if (!(ch.q == "min" ? (iv == 2 || iv == 10 || iv == 5) : (iv == 2 || iv == 11))) continue;
            if (ch.bass && mod12(P - *ch.bass) == 1) continue;
            bool clash = false; for (size_t j = 0; j < cur.size(); j++) if (j != k && (std::abs(cur[j] - P) <= 1 || std::abs(cur[j] - P) == 13)) clash = true;
            if (clash) continue;
            const double score = 1 + (k == cur.size() - 1 ? 1 : 0) + (d < 0 ? .5 : 0);
            if (!have || score > bscore) { have = true; bX = X; bP = P; bscore = score; }
        }
    }
    if (!have) return std::nullopt;
    Inner t; t.beats = beats; t.from = bX; t.to = bP;
    for (int x : cur) t.notes.push_back(x == bX ? bP : x);
    std::sort(t.notes.begin(), t.notes.end());
    return t;
}

// ----------------------------------------------------------------- lead-in ----
// A short chord that takes the end of the bar before its target.
Chord Engine::makeLeadIn(const Project& p, const Chord& target, const Chord& prev, Rng& rand) const {
    const int t = degOf(p, target); const bool minor = target.q == "min";
    std::vector<const LeadOption*> opts; std::vector<double> w;
    for (auto& o : st->leadIns) if (mod12(p.key + t + o.offset) != prev.root) { opts.push_back(&o); w.push_back(o.w); }
    Chord ch; ch.kind = "approach";
    const int k = pickWeighted(w, rand);
    if (k >= 0) {
        const LeadOption& o = *opts[(size_t) k];
        const bool mn = o.followQuality && minor;
        ch.root = mod12(p.key + t + o.offset); ch.q = mn ? "min" : "maj"; ch.ext = mn ? o.sMinor : o.s; ch.iv = mn ? o.ivMinor : o.iv; ch.sharp = o.sharp;
    } else { ch.root = mod12(target.root - 1); ch.iv = { 0, 4, 7 }; ch.sharp = true; }
    ch.beats = std::max(1, std::min(p.leadBeats, p.bpb - 1));
    return ch;
}

// -------------------------------------------------------------------- roll ----
double Engine::sectionBars(const Project& p, const Section& sec) { double b = 0; for (auto& s : sec.slots) b = b + s.beats; return b / p.bpb; }
char Engine::slotBassType(const Project& p, const Section& sec, int i) const {
    const Chord& ch = *sec.slots[(size_t) i].chord; const int b = mod12(ch.bass.value_or(ch.root) - p.key);
    if (b != degOf(p, ch)) return b == 0 ? 'T' : b == kFifthDeg ? 'F' : 'R';
    for (int j : { i - 1, i + 1 }) {
        if (j < 0 || j >= (int) sec.slots.size() || !sec.slots[(size_t) j].chord) continue;
        const Chord& o = *sec.slots[(size_t) j].chord;
        if (o.bass && mod12(*o.bass) != o.root && ch.bass && mod12(*o.bass) == mod12(*ch.bass)) return b == 0 ? 'T' : 'F';
    }
    return 'R';
}
std::optional<Cand> Engine::rerollCandidate(const Project& p, const Section& sec, int i, char bt, Rng& rand) const {
    const int n = (int) sec.slots.size();
    const Cand cur = asCand(p, *sec.slots[(size_t) i].chord);
    std::optional<Cand> prev, next;
    if (i > 0) prev = asCand(p, *sec.slots[(size_t) i - 1].chord);
    if (i + 1 < n) next = asCand(p, *sec.slots[(size_t) i + 1].chord);
    if ((sec.role == "home" && i == 0) || (sec.role == "return" && i == n - 1)) return std::nullopt;     // these stay on the tonic
    const std::optional<unsigned> P = i > 0 ? std::optional<unsigned>(relPcs(p, *sec.slots[(size_t) i - 1].chord)) : std::nullopt;
    const std::optional<unsigned> N = i + 1 < n ? std::optional<unsigned>(relPcs(p, *sec.slots[(size_t) i + 1].chord)) : std::nullopt;
    auto joins = [&](const Cand& c) {
        const int bd = bassDeg(c, bt);
        for (auto* e : palette(p, c, bd != c.deg, bd)) { const unsigned S = extPcs(c, *e, bd); if ((!P || (S & *P)) && (!N || (S & *N))) return true; }
        return false;
    };
    std::vector<Cand> cands;
    for (auto& c : vocab(p.mode))
        if (ckey(c) != ckey(cur) && (!prev || ckey(c) != ckey(*prev)) && (!next || ckey(c) != ckey(*next)) && (bt == 'R' || pedalOk(c, bt)) && joins(c)) cands.push_back(c);
    if (cands.empty()) return std::nullopt;
    std::vector<std::string> inSec; for (auto& s : sec.slots) if (s.chord) inSec.push_back(ckey(asCand(p, *s.chord)));
    const bool minorMode = st->modeOr(p.mode).minor;
    std::vector<double> w;
    for (auto& c : cands) {
        const bool in = std::find(inSec.begin(), inSec.end(), ckey(c)) != inSec.end();
        w.push_back((in ? 2 : 1) * (1 + (prev ? commonTones(*prev, c) : 1)) * (1 + (next ? commonTones(c, *next) : 1)) * (c.deg == 4 ? .5 : 1)
                    * (!minorMode && c.q == "min" ? .4 + 1.6 * p.mood.wistful : 1));
    }
    Cand c = cands[(size_t) pickWeighted(w, rand)]; c.kind = "diatonic";
    return c;
}

// Roll a section. `only` rerolls just those chords.
// Locked slots are never touched: notes, bass, suspension, tail and lead-in stay.
void Engine::rollSection(Project& p, Section& sec, const std::vector<int>* only, uint32_t seed) const {
    Rng rand(seed);
    if (p.majorModesOnly && st->modeOr(p.mode).minor) {      // a minor mode is replaced before the roll
        const ModeDef* d = st->mode(st->defaultMode);
        if (!d || d->minor) for (auto& m : st->modes) if (!m.minor) { d = &m; break; }
        if (d && !d->minor) p.mode = d->id;
    }
    if (!st->mode(p.mode)) p.mode = st->defaultMode;
    const Mood& m = p.mood; const Rules& R = st->rules;
    const double beats = p.bpb * std::max(1, sec.chordBars ? sec.chordBars : 1);
    std::vector<Cand> seq; std::vector<char> types; int n = 0;
    if (only) for (auto& s : sec.slots) if (!s.chord) { only = nullptr; break; }      // nothing to re-roll around yet
    if (only && sec.slots.empty()) only = nullptr;
    auto inOnly = [&](int i) { return only && std::find(only->begin(), only->end(), i) != only->end(); };

    if (only) {
        n = (int) sec.slots.size();
        for (auto& s : sec.slots) seq.push_back(asCand(p, *s.chord));
        const PlanDef* pl = sec.plan ? st->plan(sec.plan->id) : nullptr;
        if (pl && (n == 4 || n == 8)) types = pl->types(n);
        else for (int i = 0; i < n; i++) types.push_back(slotBassType(p, sec, i));
        if (sec.role == "return") types[(size_t) n - 1] = 'T';
        for (int i : *only) {
            if (i < 0 || i >= n || sec.slots[(size_t) i].locked) continue;
            if (auto c = rerollCandidate(p, sec, i, types[(size_t) i], rand)) seq[(size_t) i] = *c;
        }
    } else {
        n = sec.length == 4 ? 4 : 8;
        const std::vector<Slot> old = sec.slots;
        std::vector<std::optional<Cand>> locked((size_t) n);
        for (int i = 0; i < n; i++)
            if ((size_t) i < old.size() && old[(size_t) i].locked && old[(size_t) i].chord && !old[(size_t) i].chord->notes.empty()) locked[(size_t) i] = asCand(p, *old[(size_t) i].chord);
        const std::string planId = choosePlan(p, sec, rand);
        Skeleton sk;
        for (int tries = 0; tries < 40; tries++) {
            sk = chooseSkeleton(p, sec, n, planId, locked, rand);
            borrowOne(p, sec, sk.seq, sk.types, [&](int i) { return (bool) locked[(size_t) i]; }, rand);
            std::vector<int> bd; std::vector<std::optional<unsigned>> giv;
            for (int i = 0; i < n; i++) {
                bd.push_back(locked[(size_t) i] ? -1 : bassDeg(sk.seq[(size_t) i], sk.types[(size_t) i]));
                giv.push_back(locked[(size_t) i] ? std::optional<unsigned>(relPcs(p, *old[(size_t) i].chord)) : std::nullopt);
            }
            Rng trialRng(1);
            const Colored trial = colorize(p, sk.seq, bd, giv, false, trialRng, sec.role == "return");
            bool dull = false;                                    // never the same chord three times running
            for (int i = 2; i < n; i++) if (ckey(sk.seq[(size_t) i]) == ckey(sk.seq[(size_t) i - 1]) && ckey(sk.seq[(size_t) i]) == ckey(sk.seq[(size_t) i - 2])) dull = true;
            std::vector<std::string> distinct;
            for (auto& c : sk.seq) if (c.kind != "borrowed" && std::find(distinct.begin(), distinct.end(), ckey(c)) == distinct.end()) distinct.push_back(ckey(c));
            if ((int) distinct.size() > R.maxDistinct) dull = true;      // small vocabulary
            if (trial.ok && !dull) break;
        }
        seq = sk.seq; types = sk.types;
        const PlanDef* pl = st->plan(planId);
        sec.plan = PlanRef{ planId, pl ? pl->label : planId };
        sec.from = sk.from;
        sec.slots.clear();
        for (int i = 0; i < n; i++) {
            if (locked[(size_t) i]) sec.slots.push_back(old[(size_t) i]);
            else { Slot s; s.beats = beats; sec.slots.push_back(s); }
        }
        if (sec.role == "return") types[(size_t) n - 1] = 'T';
    }
    auto fixed = [&](int i) -> bool {
        const Slot& s = sec.slots[(size_t) i];
        return only ? (!inOnly(i) || s.locked) : (s.locked && s.chord && !s.chord->notes.empty());
    };

    // Bass degrees and color.
    std::vector<int> bdegs; std::vector<std::optional<unsigned>> given;
    for (int i = 0; i < n; i++) {
        if (fixed(i)) { const Chord& ch = *sec.slots[(size_t) i].chord; bdegs.push_back(mod12(ch.bass.value_or(ch.root) - p.key)); given.push_back(relPcs(p, ch)); }
        else { bdegs.push_back(bassDeg(seq[(size_t) i], types[(size_t) i])); given.push_back(std::nullopt); }
    }
    const Colored col = colorize(p, seq, bdegs, given, sec.role != "return", rand, sec.role == "return");
    for (int i = 0; i < n; i++) {
        if (fixed(i)) continue;
        const Cand& c = seq[(size_t) i]; const ExtDef* e = col.exts[(size_t) i];
        Chord ch; ch.root = mod12(p.key + c.deg); ch.q = c.q; ch.ext = e ? e->s : ""; ch.iv = e ? e->iv : triad(c.q); ch.kind = c.kind.empty() ? "diatonic" : c.kind;
        sec.slots[(size_t) i].chord = ch;
    }
    // Lead-ins belong to the chord they lead into. None over a drone.
    for (int i = 0; i < n; i++) {
        Slot& slot = sec.slots[(size_t) i];
        if (fixed(i)) continue;
        slot.leadIn.reset();
        if (i == 0 || p.bpb < 2 || slot.beats < 2) continue;
        const Chord& before = *sec.slots[(size_t) i - 1].chord;
        if (before.root == slot.chord->root || bdegs[(size_t) i] == bdegs[(size_t) i - 1]) continue;
        if (rand.next() < m.leadins) slot.leadIn = makeLeadIn(p, *slot.chord, before, rand);
    }
    // Voice in playing order; fixed slots keep their stored notes and bass.
    const Section* home = homeSection(p, &sec);
    const Slot* hl = home && !home->slots.empty() ? &home->slots.back() : nullptr;
    std::vector<int> prevStore; const std::vector<int>* prevNotes = nullptr;
    if (sec.role != "home" && hl && hl->chord && !hl->chord->notes.empty()) { prevStore = hl->chord->notes; prevNotes = &prevStore; }
    int prevBass = tonicBass(p.key);
    for (int i = 0; i < n; i++) {
        Slot& slot = sec.slots[(size_t) i]; const bool isFixed = fixed(i);
        if (slot.leadIn && !(isFixed && !slot.leadIn->notes.empty())) {
            slot.leadIn->notes = voice(*slot.leadIn, prevNotes);
            slot.leadIn->bass = bassNote(slot.leadIn->root, prevBass);
        }
        if (slot.leadIn) { prevStore = slot.leadIn->notes; prevNotes = &prevStore; prevBass = slot.leadIn->bass.value_or(prevBass); }
        if (!isFixed) {
            const int pc = mod12(p.key + bdegs[(size_t) i]);
            slot.chord->bass = bdegs[(size_t) i] == 0 ? tonicBass(p.key) : bassNote(pc, prevBass);
            slot.chord->notes = voice(*slot.chord, prevNotes);
        }
        prevStore = slot.chord->notes; prevNotes = &prevStore; prevBass = slot.chord->bass.value_or(prevBass);
    }
    // Inner movement. Suspensions land where the tension lets go; slot 0 looks
    // back at the last slot, for when the section loops.
    auto scaleOf = [&](const Chord& ch) {
        const std::vector<int>* sc = parentScale(p.mode, asCand(p, ch)); if (!sc) sc = &st->modeOr(p.mode).steps;
        std::vector<int> out; for (int s : *sc) out.push_back(mod12(s + p.key)); return out;
    };
    auto ten = [&](int i) { const Chord& ch = *sec.slots[(size_t) i].chord; return tension(asCand(p, ch), mod12(ch.bass.value_or(ch.root) - p.key)); };
    auto touch = [&](int i) { return !fixed(i) || (only && !sec.slots[(size_t) i].locked && (inOnly((i + n - 1) % n) || inOnly((i + 1) % n))); };
    // No more than about half the chords get one, and the same suspension is not heard more than twice.
    std::vector<std::pair<double, int>> ord;
    for (int i = 0; i < n; i++) ord.push_back({ rand.next(), i });
    std::stable_sort(ord.begin(), ord.end(), [](auto& a, auto& b) { return a.first < b.first; });
    auto susKey = [](const Inner& s) { return mod12(s.from) * 12 + mod12(s.to); };
    std::map<int, int> seen;
    int susCount = 0;
    const int cap = (int) std::ceil(n * (R.susCapBase + R.susCapSlope * m.inner));
    for (int i = 0; i < n; i++) if (!touch(i) && sec.slots[(size_t) i].sus) { susCount++; seen[susKey(*sec.slots[(size_t) i].sus)]++; }
    for (auto& o : ord) {
        const int i = o.second; Slot& slot = sec.slots[(size_t) i];
        if (!touch(i)) continue;
        slot.sus.reset();
        if (slot.leadIn || n < 2 || m.inner <= 0 || susCount >= cap) continue;
        const int pi = (i + n - 1) % n; const Chord& prev = *sec.slots[(size_t) pi].chord;
        if (prev.root == slot.chord->root) continue;
        const bool release = ten(i) <= ten(pi) - R.susReleaseDrop, last = sec.role == "return" && i == n - 1;
        const double pr = last ? R.susLastBase + R.susLastSlope * m.inner : release ? std::min(R.susReleaseMax, m.inner * R.susReleaseMul) : m.inner * R.susOtherMul;
        if (rand.next() >= pr) continue;
        auto s = findSuspension(prev.notes, slot.chord->notes, *slot.chord, std::min(std::max(1.0, std::floor(slot.beats / 2)), (double) p.bpb));
        if (!s || seen[susKey(*s)] >= R.susRepeatMax) continue;
        seen[susKey(*s)]++; slot.sus = s; susCount++;
    }
    for (int i = 0; i < n; i++) {
        Slot& slot = sec.slots[(size_t) i];
        if (!touch(i)) continue;
        slot.tail.reset();
        if (m.inner <= 0 || n < 2 || slot.beats < 2 || (i == n - 1 && sec.role == "return")) continue;
        const Slot& nx = sec.slots[(size_t) ((i + 1) % n)];
        if (nx.sus || nx.leadIn || (slot.sus && slot.beats < 2 * p.bpb)) continue;
        if (rand.next() < m.inner * R.tailChance)
            slot.tail = findTail(slot.chord->notes, nx.chord->notes, *slot.chord, scaleOf(*slot.chord), std::max(1.0, std::floor(slot.beats / (slot.sus ? 4 : 2))));
    }
}

static int roleIndex(const std::string& r) {
    static const char* roles[] = { "home", "lift", "away", "turn", "return" };
    for (int i = 0; i < 5; i++) if (r == roles[i]) return i;
    return -1;
}
void Engine::rollAll(Project& p, const std::function<uint32_t()>& seeds) const {
    std::vector<Section*> order; for (auto& s : p.sections) order.push_back(&s);
    std::stable_sort(order.begin(), order.end(), [](Section* a, Section* b) { return roleIndex(a->role) < roleIndex(b->role); });
    for (auto* s : order) rollSection(p, *s, nullptr, seeds());
}
void Engine::refitMeter(Project& p, int bpb) const {
    bpb = std::max(1, std::min(16, bpb));
    for (auto& sec : p.sections) {
        const double beats = bpb * std::max(1, sec.chordBars);
        for (auto& s : sec.slots) {
            s.beats = beats;
            if (s.sus) s.sus->beats = std::min(std::max(1.0, std::floor(beats / 2)), (double) bpb);
            if (s.tail) s.tail->beats = std::max(1.0, std::floor(beats / (s.sus ? 4 : 2)));
            if (s.leadIn) s.leadIn->beats = std::max(1, std::min(p.leadBeats, bpb - 1));
        }
    }
    p.bpb = bpb;
}

Section Engine::newSection(const std::string& role, const IdSource& ids) const {
    Section s; s.id = ids(); s.role = role; s.name = st->roleName(role);
    const RoleDef* r = st->role(role); s.length = r ? r->length : 8;
    return s;
}
Project Engine::newProject(const IdSource& ids) const {
    Project p; p.style = st->id; p.mode = st->defaultMode; p.tempo = st->defaultTempo; p.bpb = st->defaultBpb; p.tie = st->defaultTie;
    for (auto& sp : st->spectra) if (double* v = p.mood.byId(sp.id)) *v = sp.def;
    for (auto& r : st->defaultSections) p.sections.push_back(newSection(r, ids));
    for (auto& b : st->defaultArrangement) p.arrangement.push_back({ ids(), p.sections[(size_t) b.first].id, b.second });
    return p;
}

}  // namespace kindred
