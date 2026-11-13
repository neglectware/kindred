#include "Style.h"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace kindred {

std::vector<char> PlanDef::types(int n) const {
    std::vector<char> t((size_t) n, a);
    for (int i = 0; i < n; i++) {
        if (kind == Half) t[(size_t) i] = i < n / 2.0 ? a : b;
        else if (kind == Tail) t[(size_t) i] = i < n - (n > 4 ? tailLong : tailShort) ? a : b;
    }
    return t;
}
const ModeDef* Style::mode(const std::string& mid) const { for (auto& m : modes) if (m.id == mid) return &m; return nullptr; }
const ModeDef& Style::modeOr(const std::string& mid) const { auto* m = mode(mid); return m ? *m : modes.front(); }
const PlanDef* Style::plan(const std::string& pid) const { for (auto& p : plans) if (p.id == pid) return &p; return nullptr; }
const RoleDef* Style::role(const std::string& rid) const { for (auto& r : roles) if (r.id == rid) return &r; return nullptr; }
const SpectrumDef* Style::spectrum(const std::string& sid) const { for (auto& s : spectra) if (s.id == sid) return &s; return nullptr; }
const std::vector<CellDef>& Style::cellList(const std::string& modeId, const std::string& kind) const {
    static const std::vector<CellDef> none;
    auto m = cells.find(modeId); if (m == cells.end()) return none;
    auto k = m->second.find(kind); return k == m->second.end() ? none : k->second;
}
std::string Style::roleName(const std::string& rid) const { auto* r = role(rid); return r ? r->name : rid; }

static std::vector<int> ints(const Json& j) { std::vector<int> v; for (auto& x : j.items()) v.push_back(x.integer()); return v; }
[[noreturn]] static void bad(const std::string& what) { throw std::runtime_error("Style file: " + what); }
static char bassType(const Json& j, const std::string& where) {
    const std::string s = j.stringOr("");
    if (s != "T" && s != "F" && s != "R") bad(where + ": a bass type must be \"T\" (tonic drone), \"F\" (drone on the fifth) or \"R\" (chord root)");
    return s[0];
}

std::shared_ptr<const Style> Style::fromJson(const Json& j) {
    if (!j.isObject()) bad("the file is not a JSON object");
    auto st = std::make_shared<Style>();
    Style& s = *st;
    s.format = j["format"].integer(1);
    if (s.format != 1) bad("format " + std::to_string(s.format) + " is not supported by this version");
    s.id = j["id"].stringOr(""); s.name = j["name"].stringOr(s.id); s.description = j["description"].stringOr("");
    if (s.id.empty()) bad("\"id\" is missing");

    for (auto& m : j["modes"].items()) {
        ModeDef d; d.id = m["id"].stringOr(""); d.name = m["name"].stringOr(d.id); d.steps = ints(m["steps"]); d.minor = m["minor"].boolean();
        if (d.id.empty() || d.steps.size() != 7) bad("mode \"" + d.id + "\" needs an id and 7 steps");
        for (auto& b : m["planBias"].members()) d.planBias[b.first] = b.second.number(1);
        for (auto& b : m["borrow"].items()) {
            if (b.stringOr("") == "iv") d.borrowMinorFour = true; else if (b.stringOr("") == "bVII") d.borrowFlatSeven = true;
            else bad("mode \"" + d.id + "\": borrow entries are \"iv\" or \"bVII\"");
        }
        s.modes.push_back(d);
    }
    if (s.modes.empty()) bad("\"modes\" is empty");

    for (auto& q : j["extensions"].members())
        for (auto& e : q.second.items()) {
            ExtDef d{ e["s"].stringOr(""), ints(e["iv"]), e["t"].stringOr("plain") };
            if (d.iv.size() < 3) bad("extension \"" + d.s + "\" needs at least 3 intervals");
            s.extensions[q.first].push_back(d);
        }
    if (!s.extensions.count("maj") || !s.extensions.count("min")) bad("\"extensions\" needs \"maj\" and \"min\" lists");

    for (auto& w : j["colorWeights"].members()) {
        ColorWeight c; const Json& o = w.second;
        c.c0 = o["base"].number(0); c.cr = o["color"].number(0); c.cinv = o["plain"].number(0);
        for (auto& d : o["degree"].members()) c.deg[d.first] = d.second.number(1);
        c.degDefault = o["degreeDefault"].number(1); c.degOwnRootOnly = o["degreeOwnRootOnly"].boolean(); c.drone = o["overDrone"].number(1);
        s.colorWeights[w.first] = c;
    }
    if (!s.colorWeights.count("default")) bad("\"colorWeights\" needs a \"default\" entry");

    for (auto& t : j["tokens"].members()) {
        Cand c; c.deg = t.second[0].integer(); c.q = t.second[1].stringOr("maj");
        if (c.q != "maj" && c.q != "min") bad("token \"" + t.first + "\": quality must be \"maj\" or \"min\"");
        s.tokens[t.first] = c;
    }
    for (auto& m : j["cells"].members()) {
        if (!s.mode(m.first)) bad("cells are given for an unknown mode \"" + m.first + "\"");
        for (auto& k : m.second.members())
            for (auto& c : k.second.items()) {
                CellDef d; d.id = c[0].stringOr(""); d.w = c[1].number(1); d.d = c[2].number(0);
                size_t a = 0;
                while (a <= d.id.size()) {
                    size_t b = d.id.find(' ', a); if (b == std::string::npos) b = d.id.size();
                    const std::string tok = d.id.substr(a, b - a);
                    if (!tok.empty()) { auto it = s.tokens.find(tok); if (it == s.tokens.end()) bad("cell \"" + d.id + "\" uses an unknown chord \"" + tok + "\""); d.chords.push_back(it->second); }
                    a = b + 1;
                }
                if (d.chords.size() != 4) bad("cell \"" + d.id + "\" must have 4 chords");
                s.cells[m.first][k.first].push_back(d);
            }
    }
    for (auto& m : s.modes) {
        if (s.cellList(m.id, "home").empty() || s.cellList(m.id, "lift").empty() || s.cellList(m.id, "close").empty())
            bad("mode \"" + m.id + "\" needs \"home\", \"lift\" and \"close\" cells");
    }

    for (auto& p : j["plans"].items()) {
        PlanDef d; d.id = p["id"].stringOr(""); d.label = p["label"].stringOr(d.id);
        const Json& pt = p["pattern"];
        if (pt.has("all")) { d.kind = PlanDef::All; d.a = d.b = bassType(pt["all"], d.id); }
        else if (pt.has("half")) { d.kind = PlanDef::Half; d.a = bassType(pt["half"][0], d.id); d.b = bassType(pt["half"][1], d.id); }
        else if (pt.has("tail")) { d.kind = PlanDef::Tail; d.a = bassType(pt["tail"][0], d.id); d.b = bassType(pt["tail"][1], d.id);
            d.tailLong = pt["long"].integer(2); d.tailShort = pt["short"].integer(1); }
        else bad("plan \"" + d.id + "\" needs a pattern: \"all\", \"half\" or \"tail\"");
        s.plans.push_back(d);
    }
    if (s.plans.empty()) bad("\"plans\" is empty");

    for (auto& r : j["roles"].items()) {
        RoleDef d; d.id = r["id"].stringOr(""); d.name = r["name"].stringOr(d.id); d.length = r["length"].integer(8) == 4 ? 4 : 8;
        if (d.id != "home" && d.id != "lift" && d.id != "away" && d.id != "turn" && d.id != "return")
            bad("role \"" + d.id + "\" is not one of home, lift, away, turn, return");
        for (auto& p : r["plans"].items()) { if (!s.plan(p.stringOr(""))) bad("role \"" + d.id + "\" lists an unknown plan"); d.plans.push_back(p.string()); }
        if (d.plans.empty()) bad("role \"" + d.id + "\" has no plans");
        for (auto& w : r["planWeights"].members()) {
            std::vector<PolyTerm> terms;
            for (auto& t : w.second.items()) terms.push_back({ t[0].number(0), t[1].integer(0), t[2].integer(0) });
            d.planWeights[w.first] = terms;
        }
        s.roles.push_back(d);
    }
    for (const char* need : { "home", "lift", "away", "turn", "return" }) if (!s.role(need)) bad(std::string("role \"") + need + "\" is missing");

    for (auto& x : j["spectra"].items()) {
        SpectrumDef d; d.id = x["id"].stringOr(""); d.name = x["name"].stringOr(d.id); d.low = x["low"].stringOr(""); d.high = x["high"].stringOr("");
        d.lowHint = x["lowHint"].stringOr(""); d.highHint = x["highHint"].stringOr("");
        d.min = x["min"].number(0); d.max = x["max"].number(1); d.def = x["default"].number(d.min);
        if (!(d.max > d.min) || d.min < 0 || d.max > 1) bad("spectrum \"" + d.id + "\": needs 0 <= min < max <= 1");
        s.spectra.push_back(d);
    }
    for (const char* need : { "wistful", "moving", "color", "inner", "surprise", "leadins", "edge" }) if (!s.spectrum(need)) bad(std::string("spectrum \"") + need + "\" is missing");

    for (auto& t : j["tension"].members()) { if (t.first == "default") s.tensionDefault = t.second.number(.5); else s.tension[std::atoi(t.first.c_str())] = t.second.number(.5); }
    s.tonicDroneExclude = ints(j["drone"]["tonicExcludes"]); s.fifthDroneAllow = ints(j["drone"]["fifthAllows"]);
    for (auto& l : j["leadIns"].items()) {
        LeadOption o; o.w = l["w"].number(1); o.offset = l["offset"].integer(); o.followQuality = l["q"].stringOr("maj") == "target";
        o.s = l["ext"].stringOr(""); o.sMinor = l["extMinor"].stringOr(o.s); o.iv = ints(l["iv"]); o.ivMinor = l.has("ivMinor") ? ints(l["ivMinor"]) : o.iv; o.sharp = l["sharp"].boolean();
        if (o.iv.size() < 3) bad("a lead-in needs at least 3 intervals");
        s.leadIns.push_back(o);
    }
    for (auto& b : j["edge"]["borrow"].items()) s.edgeBorrow.push_back({ b[0].integer(), b[1].stringOr("maj") });
    s.extendedName = j["extended"]["name"].stringOr(s.extendedName); s.extendedHint = j["extended"]["hint"].stringOr("");

    Rules& r = s.rules; const Json& R = j["rules"];
    struct I { const char* k; int* p; }; struct D { const char* k; double* p; };
    for (auto e : { I{"voiceLo",&r.voiceLo}, I{"voiceHi",&r.voiceHi}, I{"voiceCenter",&r.voiceCenter}, I{"voiceMaxSpan",&r.voiceMaxSpan}, I{"voiceLowLimit",&r.voiceLowLimit},
                    I{"voiceLowMinGap",&r.voiceLowMinGap}, I{"bassLo",&r.bassLo}, I{"bassHi",&r.bassHi}, I{"maxDistinct",&r.maxDistinct}, I{"susRepeatMax",&r.susRepeatMax},
                    I{"chordVelocity",&r.chordVelocity}, I{"bassVelocity",&r.bassVelocity}, I{"edgeMaxBorrowed",&r.edgeMaxBorrowed} })
        if (R.has(e.k)) *e.p = R[e.k].integer(*e.p);
    for (auto e : { D{"borrowChance",&r.borrowChance}, D{"borrowChanceAway",&r.borrowChanceAway}, D{"cellDriveSlope",&r.cellDriveSlope}, D{"cellDrivePivot",&r.cellDrivePivot},
                    D{"cellMinWeight",&r.cellMinWeight}, D{"minorBase",&r.minorBase}, D{"minorSlope",&r.minorSlope}, D{"pedalMisfit",&r.pedalMisfit}, D{"lockedMatch",&r.lockedMatch},
                    D{"lockedMiss",&r.lockedMiss}, D{"homePlanRepeat",&r.homePlanRepeat}, D{"connectBoost",&r.connectBoost}, D{"susCapBase",&r.susCapBase}, D{"susCapSlope",&r.susCapSlope},
                    D{"susReleaseDrop",&r.susReleaseDrop}, D{"susLastBase",&r.susLastBase}, D{"susLastSlope",&r.susLastSlope}, D{"susReleaseMul",&r.susReleaseMul},
                    D{"susReleaseMax",&r.susReleaseMax}, D{"susOtherMul",&r.susOtherMul}, D{"tailChance",&r.tailChance}, D{"edgeColorWeight",&r.edgeColorWeight},
                    D{"edgeBorrowChance",&r.edgeBorrowChance}, D{"edgeDisconnect",&r.edgeDisconnect} })
        if (R.has(e.k)) *e.p = R[e.k].number(*e.p);
    if (R.has("seamKeepsNeighbor")) r.seamKeepsNeighbor = R["seamKeepsNeighbor"].boolean();
    for (auto& m : R.members()) {
        bool known = m.first == "seamKeepsNeighbor";
        for (const char* k : { "voiceLo","voiceHi","voiceCenter","voiceMaxSpan","voiceLowLimit","voiceLowMinGap","bassLo","bassHi","maxDistinct","susRepeatMax","chordVelocity","bassVelocity",
              "edgeMaxBorrowed","borrowChance","borrowChanceAway","cellDriveSlope","cellDrivePivot","cellMinWeight","minorBase","minorSlope","pedalMisfit","lockedMatch","lockedMiss",
              "homePlanRepeat","connectBoost","susCapBase","susCapSlope","susReleaseDrop","susLastBase","susLastSlope","susReleaseMul","susReleaseMax","susOtherMul","tailChance",
              "edgeColorWeight","edgeBorrowChance","edgeDisconnect" }) if (m.first == k) known = true;
        if (!known) bad("unknown rule \"" + m.first + "\"");
    }
    if (r.voiceLo < 24 || r.voiceHi > 108 || r.voiceHi - r.voiceLo < 24) bad("voicing range must span at least two octaves inside 24..108");
    if (r.bassLo < 12 || r.bassHi - r.bassLo < 11) bad("bass range must span at least an octave");

    const Json& df = j["defaults"];
    s.defaultMode = df["mode"].stringOr(s.modes.front().id); if (!s.mode(s.defaultMode)) bad("defaults.mode is not a mode of this style");
    s.defaultTempo = df["tempo"].number(60); s.defaultBpb = df["bpb"].integer(4); s.defaultTie = df.has("tie") ? df["tie"].boolean() : true;
    for (auto& x : df["sections"].items()) { if (!s.role(x.stringOr(""))) bad("defaults.sections lists an unknown role"); s.defaultSections.push_back(x.string()); }
    if (s.defaultSections.empty()) s.defaultSections = { "home", "lift", "away", "return" };
    for (auto& b : df["arrangement"].items()) {
        int idx = b[0].integer(-1);
        if (idx < 0 || idx >= (int) s.defaultSections.size()) bad("defaults.arrangement refers to a section that is not in defaults.sections");
        s.defaultArrangement.push_back({ idx, std::max(1, b[1].integer(8)) });
    }
    return st;
}

}  // namespace kindred
