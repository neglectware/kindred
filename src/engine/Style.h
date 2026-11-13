// A style is the data the engine composes from: modes, chord cells, bass plans,
// color palette, weights and limits. See docs/styles.md for the file format.
#pragma once
#include "Json.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace kindred {

struct Cand { int deg = 0; std::string q = "maj"; std::string kind = "diatonic"; };
inline std::string ckey(const Cand& c) { return std::to_string(c.deg) + c.q; }

struct ModeDef {
    std::string id, name;
    std::vector<int> steps;                 // 7 scale steps in semitones
    bool minor = false;
    std::map<std::string, double> planBias; // multiplies a plan's weight; "*" = every other plan
    bool borrowMinorFour = false;           // the minor iv may replace IV before I
    bool borrowFlatSeven = false;           // bVII may replace V or ii before I or IV
};
struct ExtDef { std::string s; std::vector<int> iv; std::string t; };
struct CellDef { std::string id; double w = 1, d = 0; std::vector<Cand> chords; };
struct PlanDef {
    std::string id, label;
    enum Kind { All, Half, Tail } kind = All;
    char a = 'T', b = 'R';
    int tailLong = 2, tailShort = 1;
    std::vector<char> types(int n) const;
};
struct PolyTerm { double c = 0; int powD = 0, powM = 0; };       // c * D^powD * M^powM, D = 1 - M
struct RoleDef {
    std::string id, name;
    int length = 8;
    std::vector<std::string> plans;
    std::map<std::string, std::vector<PolyTerm>> planWeights;
};
struct ColorWeight {              // w = (c0 + cr*r + cinv*(1-r)) * degree factor * drone factor
    double c0 = 0, cr = 0, cinv = 0;
    std::map<std::string, double> deg;   // key "7maj" (degree + quality) or "5" (any quality)
    double degDefault = 1;
    bool degOwnRootOnly = false;         // the degree factor applies only when the bass is the chord root
    double drone = 1;                    // factor when the chord sits over a drone
};
struct SpectrumDef { std::string id, name, low, high, lowHint, highHint; double min = 0, max = 1, def = 0; };
struct LeadOption { double w = 1; int offset = 0; bool followQuality = false; std::string s, sMinor; std::vector<int> iv, ivMinor; bool sharp = false; };
struct BorrowOption { int deg = 0; std::string q; };

struct Rules {
    // voicing
    int voiceLo = 50, voiceHi = 79, voiceCenter = 63, voiceMaxSpan = 19, voiceLowLimit = 57, voiceLowMinGap = 5;
    int bassLo = 33, bassHi = 45;
    // skeleton
    int maxDistinct = 4;
    double borrowChance = .55, borrowChanceAway = .9;
    double cellDriveSlope = 1.6, cellDrivePivot = .4, cellMinWeight = .15;
    double minorBase = .15, minorSlope = 2.2;
    double pedalMisfit = .3, lockedMatch = 8, lockedMiss = .5, homePlanRepeat = .35;
    // color
    double connectBoost = 1.2;
    // inner movement
    double susCapBase = .15, susCapSlope = .35, susReleaseDrop = .15, susLastBase = .4, susLastSlope = .6;
    double susReleaseMul = 1.2, susReleaseMax = .9, susOtherMul = .2, tailChance = .6;
    int susRepeatMax = 2;
    // output
    int chordVelocity = 72, bassVelocity = 80;
    // edge (new in Kindred; all inactive at edge 0)
    double edgeColorWeight = 1.1, edgeBorrowChance = .8, edgeDisconnect = .5;
    int edgeMaxBorrowed = 2;
    // Fixes to the prototype (each one changes a rare result, so each has a switch; false = exactly as the prototype)
    bool seamKeepsNeighbor = true;   // when the last chord is recolored to meet the first, it must still share a note with the chord before it
};

struct Style {
    int format = 1;
    std::string id, name, description;
    std::vector<ModeDef> modes;
    std::map<std::string, std::vector<ExtDef>> extensions;       // by triad quality
    std::map<std::string, ColorWeight> colorWeights;             // by extension tag; "default" for the rest
    std::map<std::string, Cand> tokens;
    std::map<std::string, std::map<std::string, std::vector<CellDef>>> cells;   // mode -> kind -> cells
    std::vector<PlanDef> plans;
    std::vector<RoleDef> roles;
    std::vector<SpectrumDef> spectra;
    std::map<int, double> tension; double tensionDefault = .5;
    std::vector<int> tonicDroneExclude, fifthDroneAllow;
    std::vector<LeadOption> leadIns;
    std::vector<BorrowOption> edgeBorrow;                        // chords the edge control may borrow
    std::string extendedName = "Extended colors", extendedHint;
    Rules rules;
    // Defaults for a new project.
    std::string defaultMode = "ionian"; double defaultTempo = 60; int defaultBpb = 4; bool defaultTie = true;
    std::vector<std::string> defaultSections;
    std::vector<std::pair<int, int>> defaultArrangement;         // (index into defaultSections, bars)

    const ModeDef* mode(const std::string& id) const;
    const ModeDef& modeOr(const std::string& id) const;          // falls back to the first mode
    const PlanDef* plan(const std::string& id) const;
    const RoleDef* role(const std::string& id) const;
    const SpectrumDef* spectrum(const std::string& id) const;
    const std::vector<CellDef>& cellList(const std::string& mode, const std::string& kind) const;
    std::string roleName(const std::string& id) const;

    // Throws std::runtime_error with a readable message if the file is not a valid style.
    static std::shared_ptr<const Style> fromJson(const Json& j);
    static std::shared_ptr<const Style> fromText(const std::string& text) { return fromJson(Json::parse(text)); }
};
using StylePtr = std::shared_ptr<const Style>;

}  // namespace kindred
