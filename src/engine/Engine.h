// Kindred harmony engine. A faithful C++ port of the v2 prototype (engine.js):
// a section is a phrase built in layers (skeleton of four-chord cells, a bass
// plan, color chosen for shared notes, close voicing, inner movement).
// Every roll is stored as explicit notes, bass notes and beats, never a seed.
// Pure C++17, no JUCE: the same code runs in the plug-in and in headless tests.
#pragma once
#include "Json.h"
#include "Style.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace kindred {

inline constexpr const char* kEngineVersion = "3.0.0";
inline constexpr int kSchema = 4;          // 1 = v0, 2 = v1, 3 = v2 web prototypes, 4 = Kindred
inline constexpr int kPPQ = 960;

inline int mod12(int n) { return ((n % 12) + 12) % 12; }

// mulberry32, the generator the prototype uses; same seed, same stream.
struct Rng {
    uint32_t a;
    explicit Rng(uint32_t seed) : a(seed) {}
    double next();
};

struct Chord {
    int root = 0;
    std::string q = "maj", ext;
    std::vector<int> iv;
    std::string kind = "diatonic";         // diatonic | borrowed | approach
    std::vector<int> notes;                // upper voices, absolute MIDI notes, ascending
    std::optional<int> bass;
    bool sharp = false;                    // spell the root with a sharp (chromatic lead-ins)
    double beats = 0;                      // lead-in chords only
};
struct Inner {                             // a suspension (from = held note) or a tail (from = the voice that moves)
    double beats = 1;
    std::vector<int> notes;
    int from = 0, to = 0;
};
struct Slot {
    bool locked = false;
    std::optional<Chord> chord;
    double beats = 4;
    std::optional<Inner> sus, tail;
    std::optional<Chord> leadIn;           // plays at the end of the slot before this one
};
struct PlanRef { std::string id, label; };
struct Section {
    std::string id, name, role = "home";
    int chordBars = 1, length = 8;
    std::string planPref = "auto";
    std::optional<PlanRef> plan;
    std::optional<std::string> from;
    std::vector<Slot> slots;
    Json extra = Json::object();           // fields this version does not use (kept for round trips)
};
struct Block { std::string id, sectionId; int bars = 8; };
struct Mood {
    double wistful = .35, moving = .5, color = .5, inner = .5, surprise = .2, leadins = 0, edge = 0;
    double* byId(const std::string& id);
    const double* byId(const std::string& id) const { return const_cast<Mood*>(this)->byId(id); }
};
struct Project {
    std::string style = "warm-ambient";
    int key = 0;
    std::string mode = "ionian";
    bool majorModesOnly = false;           // a minor mode is replaced by the style's default mode before a roll
    bool extended = false;                 // allow the palette's extended (jazz) colors
    bool tie = true;                       // hold shared notes across chord changes
    double tempo = 60;
    int bpb = 4;                           // beats (quarter notes) per bar
    Mood mood;
    int leadBeats = 1;
    bool bassTrack = true;                 // bass on its own track and channel in exported files
    std::vector<Section> sections;
    std::vector<Block> arrangement;
    Json feedback = Json::array();
    Json extra = Json::object();

    Section* section(const std::string& id);
    const Section* section(const std::string& id) const { return const_cast<Project*>(this)->section(id); }
};

struct Event {
    double t = 0, len = 0;                 // in beats
    std::vector<int> notes;
    bool bass = false, sus = false, tail = false, lead = false;
    std::string sec; int idx = -1;         // the slot this event belongs to (chord events)
    std::string tag; bool hasTag = false;  // arrangement block id
};

using IdSource = std::function<std::string()>;

class Engine {
public:
    explicit Engine(StylePtr style) : st(std::move(style)) {}
    const Style& style() const { return *st; }
    StylePtr stylePtr() const { return st; }

    // --- projects
    Project newProject(const IdSource& ids) const;
    Section newSection(const std::string& role, const IdSource& ids) const;
    // Rolls a section. `only` rerolls just those slot indexes. Locked slots are never touched.
    void rollSection(Project& p, Section& sec, const std::vector<int>* only, uint32_t seed) const;
    // Rolls every section, the verse first; `seeds` supplies one seed per section.
    void rollAll(Project& p, const std::function<uint32_t()>& seeds) const;
    std::vector<std::string> allowedPlans(const Project& p, const std::string& role) const;
    // Changes the meter without changing any chord: slot lengths follow the bar, inner beats are refitted.
    void refitMeter(Project& p, int bpb) const;

    // --- playback
    std::vector<Event> blockEvents(const Project& p, const Section& sec, double bars, double startBeat, const std::string* tag) const;
    std::vector<Event> arrangementEvents(const Project& p, double* totalBeats = nullptr) const;
    static std::vector<Event> tieEvents(const std::vector<Event>& events);
    static std::vector<Event> playable(const Project& p, const std::vector<Event>& events) { return p.tie ? tieEvents(events) : events; }
    std::vector<uint8_t> midiFile(const Project& p, const std::vector<Event>& events, bool bassSeparate) const;
    // The bass alone: a tempo track and a "Bass" track on channel 2.
    std::vector<uint8_t> midiFileBassOnly(const Project& p, const std::vector<Event>& events) const;
    static double sectionBars(const Project& p, const Section& sec);

    // --- names
    static std::string noteName(int pc, int keyPc, int deg, bool preferSharp);
    static std::string chordName(const Chord& ch, int keyPc);
    static std::string romanName(const Chord& ch, int keyPc);
    static std::string keyName(int k);
    static std::string pcName(int midiNote, int keyPc);
    std::string slotText(const Project& p, const Section& sec, size_t i) const;
    std::string sectionChart(const Project& p, const Section& sec) const;
    std::string chartText(const Project& p) const;

    // --- theory helpers (also used by the tests and the UI)
    bool isDiatonic(const std::string& mode, const Cand& c) const;
    const std::vector<int>* parentScale(const std::string& mode, const Cand& c) const;
    static Cand asCand(const Project& p, const Chord& ch);
    static unsigned chordPcs(const Chord& ch);                 // bit set of pitch classes, chord notes plus bass
    static bool sharesTone(const Chord& a, const Chord& b) { return (chordPcs(a) & chordPcs(b)) != 0; }
    bool pedalOk(const Cand& c, char bassType) const;
    static int tonicBass(int key) { return key <= 8 ? 36 + key : 24 + key; }
    // Harmonic function of a chord for display: 'T' tonic, 'S' subdominant, 'D' dominant, 'M' relative minor/mediant, 'B' borrowed, 'A' approach.
    static char functionOf(const Project& p, const Chord& ch);

    // --- storage
    // Opens any project format (prototype formats 1 to 3, or Kindred). Throws std::runtime_error.
    static Project fromJson(Json j);
    static Json toJson(const Project& p);
    // Converts older files in place to format 3, as the prototype did. Chords, notes and beats never change.
    static void migrateLegacy(Json& p);

private:
    struct Skeleton { std::vector<char> types; std::optional<std::string> from; std::vector<Cand> seq; };
    const Section* homeSection(const Project& p, const Section* sec) const;
    std::string choosePlan(const Project& p, const Section& sec, Rng& rand) const;
    double cellWeight(const Project& p, const CellDef& cell, const std::vector<char>& types, int off, const std::vector<std::optional<Cand>>& locked) const;
    Skeleton chooseSkeleton(const Project& p, const Section& sec, int n, const std::string& planId, const std::vector<std::optional<Cand>>& locked, Rng& rand) const;
    void borrowOne(const Project& p, const Section& sec, std::vector<Cand>& seq, const std::vector<char>& types, const std::function<bool(int)>& isFixed, Rng& rand) const;
    std::vector<const ExtDef*> palette(const Project& p, const Cand& c, bool foreign, int bdeg) const;
    double typeWeight(const ExtDef& e, const Cand& c, double r, bool foreign) const;
    struct Colored { std::vector<const ExtDef*> exts; bool ok = true; };
    Colored colorize(const Project& p, const std::vector<Cand>& seq, const std::vector<int>& bdegs, const std::vector<std::optional<unsigned>>& given,
                     bool wrap, Rng& rand, bool restLast) const;
    std::vector<int> voice(const Chord& ch, const std::vector<int>* prevNotes) const;
    int bassNote(int pc, int prev) const;
    int bassDeg(const Cand& c, char bt) const;
    double tension(const Cand& c, int bdeg) const;
    Chord makeLeadIn(const Project& p, const Chord& target, const Chord& prev, Rng& rand) const;
    char slotBassType(const Project& p, const Section& sec, int i) const;
    std::optional<Cand> rerollCandidate(const Project& p, const Section& sec, int i, char bt, Rng& rand) const;
    std::vector<Cand> vocab(const std::string& mode) const;
    StylePtr st;
};

// Canonical text of the musical content of a project (used to compare engines).
std::string canonText(const Project& p);
uint64_t fnv1a(const std::string& s);
uint64_t fnv1a(const std::vector<uint8_t>& b);
std::string canonEvents(const std::vector<Event>& ev);

}  // namespace kindred
