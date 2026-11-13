#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"
#include <BinaryData.h>

namespace kindred {

static std::atomic<uint64_t> timelineCounter{ 1 };

// Registers the built-in styles once, then reads the user's style folder.
static juce::StringArray& ensureStyles() {
    static juce::StringArray problems;
    static std::once_flag once;
    std::call_once(once, [] {
        auto& lib = StyleLibrary::get();
        lib.addBuiltIn(BinaryData::warmambient_json, (size_t) BinaryData::warmambient_jsonSize);      // the first style is the default
        for (int i = 0; i < BinaryData::namedResourceListSize; i++) {
            const juce::String name(BinaryData::originalFilenames[i]);
            if (!name.endsWithIgnoreCase(".json")) continue;
            int size = 0; const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], size);
            try { lib.addBuiltIn(data, (size_t) size); } catch (const std::exception& e) { problems.add(name + ": " + e.what()); }
        }
        auto folder = KindredProcessor::stylesFolder();
        for (auto& line : lib.scanFolder(folder.getFullPathName().toStdString(), [](const std::string& dir) {
                 std::vector<std::pair<std::string, std::string>> out;
                 for (auto& f : juce::File(dir).findChildFiles(juce::File::findFiles, false, "*.json"))
                     out.push_back({ f.getFileName().toStdString(), f.loadFileAsString().toStdString() });
                 return out; }))
            problems.add(juce::String::fromUTF8(line.c_str()));
    });
    return problems;
}

juce::File KindredProcessor::userFolder() {
    // macOS: ~/Library/Application Support/<company>/<product>. Windows: %APPDATA%\<company>\<product>. Linux: ~/.config/<company>/<product>.
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile(KINDRED_COMPANY_NAME).getChildFile(KINDRED_PRODUCT_NAME);
}
juce::File KindredProcessor::stylesFolder() { return userFolder().getChildFile("Styles"); }
juce::PropertiesFile& KindredProcessor::settings() {
    static std::unique_ptr<juce::PropertiesFile> file;
    static std::once_flag once;
    std::call_once(once, [] {
        juce::PropertiesFile::Options o;
        o.applicationName = KINDRED_PRODUCT_NAME; o.filenameSuffix = ".settings"; o.folderName = juce::String(KINDRED_COMPANY_NAME) + "/" + KINDRED_PRODUCT_NAME;
        o.osxLibrarySubFolder = "Application Support"; o.millisecondsBeforeSaving = 500;
        file = std::make_unique<juce::PropertiesFile>(o);
    });
    return *file;
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ "part", 1 }, "Part", StringArray{ "Chords + Bass", "Chords", "Bass" }, 0));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ "chordVel", 1 }, "Chord Velocity", 1, 127, 72));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ "bassVel", 1 }, "Bass Velocity", 1, 127, 80));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ "chordOct", 1 }, "Chord Octave", -2, 2, 0));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ "bassOct", 1 }, "Bass Octave", -2, 2, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ "humanize", 1 }, "Humanize", NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ "strum", 1 }, "Strum", NormalisableRange<float>(0.f, 1.f, 0.01f), 0.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ "gate", 1 }, "Note Length", NormalisableRange<float>(0.1f, 1.f, 0.01f), 1.f));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ "chordCh", 1 }, "Chord Channel", 1, 16, 1));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ "bassCh", 1 }, "Bass Channel", 1, 16, 2));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ "thru", 1 }, "MIDI Thru", true));
    return l;
}
static const char* kParamIds[] = { "part", "chordVel", "bassVel", "chordOct", "bassOct", "humanize", "strum", "gate", "chordCh", "bassCh", "thru" };

KindredProcessor::KindredProcessor()
    : AudioProcessor(juce::PluginHostType::getPluginLoadedAs() == wrapperType_Standalone
                         ? BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true) : BusesProperties()),
      apvts(*this, nullptr, "params", makeLayout()),
      standalone(juce::PluginHostType::getPluginLoadedAs() == wrapperType_Standalone) {
    styleProblems = ensureStyles();
    pPart = apvts.getRawParameterValue("part"); pChordVel = apvts.getRawParameterValue("chordVel"); pBassVel = apvts.getRawParameterValue("bassVel");
    pChordOct = apvts.getRawParameterValue("chordOct"); pBassOct = apvts.getRawParameterValue("bassOct"); pHumanize = apvts.getRawParameterValue("humanize");
    pStrum = apvts.getRawParameterValue("strum"); pGate = apvts.getRawParameterValue("gate"); pChordCh = apvts.getRawParameterValue("chordCh");
    pBassCh = apvts.getRawParameterValue("bassCh"); pThru = apvts.getRawParameterValue("thru");
    for (auto* id : kParamIds) apvts.addParameterListener(id, this);
    joinLink(standalone ? "" : "A", nullptr);
    startTimerHz(20);
}
KindredProcessor::~KindredProcessor() {
    stopTimer();
    for (auto* id : kParamIds) apvts.removeParameterListener(id, this);
    if (sess) sess->removeListener(this);
}

PlayParams KindredProcessor::currentParams() const {
    PlayParams pp;
    pp.part = (int) pPart->load(); pp.chordVel = (int) pChordVel->load(); pp.bassVel = (int) pBassVel->load();
    pp.chordOct = (int) pChordOct->load(); pp.bassOct = (int) pBassOct->load(); pp.humanize = pHumanize->load(); pp.strum = pStrum->load(); pp.gate = pGate->load();
    pp.chordCh = juce::jlimit(0, 15, (int) pChordCh->load() - 1); pp.bassCh = juce::jlimit(0, 15, (int) pBassCh->load() - 1);
    return pp;
}

void KindredProcessor::joinLink(const juce::String& link, const Json* state) {
    auto s = Session::join(link.toStdString());
    std::shared_ptr<Session> old;
    { std::lock_guard<std::mutex> g(latestMx); old = sess; sess = s; linkId = link; }
    if (old && old != s) old->removeListener(this);
    s->addListener(this);
    if (state) s->restoreState(*state);
    rebuild();
}
void KindredProcessor::setLink(const juce::String& link) {
    if (link == linkId) return;
    stopPreview();
    const Json mine = sess->saveState();
    auto target = Session::join(link.toStdString());
    const bool empty = target->memberCount() == 0;
    joinLink(link, empty ? &mine : nullptr);        // an empty group takes this project; a group in use keeps its own
    partTouched = true;
}
std::shared_ptr<const RenderData> KindredProcessor::renderData() const { std::lock_guard<std::mutex> g(latestMx); return latest; }

void KindredProcessor::rebuild() {
    std::lock_guard<std::mutex> g(buildMx);
    std::shared_ptr<Session> s; { std::lock_guard<std::mutex> l(latestMx); s = sess; }
    if (!s) return;
    const auto p = s->project(); const Engine eng = s->engine();
    auto rd = std::make_shared<RenderData>();
    rd->play = s->play(); rd->params = currentParams(); rd->tempo = p->tempo; rd->bpb = p->bpb;
    auto secIndex = [&](const std::string& id) { for (size_t i = 0; i < p->sections.size(); i++) if (p->sections[i].id == id) return (int) i; return -1; };
    auto blockIndex = [&](const std::string& id) { for (size_t i = 0; i < p->arrangement.size(); i++) if (p->arrangement[i].id == id) return (int) i; return -1; };
    auto marks = [&](Timeline& tl, const std::vector<Event>& ev) {
        for (auto& e : ev) if (!e.bass) tl.marks.push_back({ e.t, e.len, secIndex(e.sec), e.idx, e.hasTag ? blockIndex(e.tag) : -1, e.sus ? 's' : e.tail ? 't' : e.lead ? 'l' : 'c' });
        std::stable_sort(tl.marks.begin(), tl.marks.end(), [](const TimeMark& a, const TimeMark& b) { return a.t < b.t; });
    };
    {
        double total = 0; const auto ev = eng.arrangementEvents(*p, &total);
        rd->arrangement.total = total; buildNotes(rd->arrangement, Engine::playable(*p, ev), rd->params); marks(rd->arrangement, ev);
        rd->arrangement.id = timelineCounter++;
    }
    if (const Section* sec = p->section(rd->play.sectionId)) {
        bool complete = !sec->slots.empty(); for (auto& sl : sec->slots) if (!sl.chord) complete = false;
        if (complete) {
            const double bars = Engine::sectionBars(*p, *sec); const auto ev = eng.blockEvents(*p, *sec, bars, 0, nullptr);
            rd->section.total = bars * p->bpb; buildNotes(rd->section, Engine::playable(*p, ev), rd->params); marks(rd->section, ev);
        }
        rd->sectionIndex = secIndex(sec->id);
    }
    rd->section.id = timelineCounter++;
    { const juce::SpinLock::ScopedLockType l(exLock); incoming = rd; fresh = true; }
    { std::lock_guard<std::mutex> l(latestMx); latest = rd; }
}
void KindredProcessor::sessionChanged() { rebuild(); }
void KindredProcessor::parameterChanged(const juce::String& id, float) {
    if (id == "part" && !settingPartInternally) partTouched = true;
    paramsDirty.store(true);
}
void KindredProcessor::setPart(int part, bool fromUser) {
    if (auto* p = apvts.getParameter("part")) {
        settingPartInternally = !fromUser;
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) part)); p->endChangeGesture();
        settingPartInternally = false;
        if (fromUser) partTouched = true;
    }
}

void KindredProcessor::timerCallback() {
    ticks++;
    if (paramsDirty.exchange(false)) rebuild();
    if (previewEnded.exchange(false)) stopPreview();
    const auto p = sess->project();
    // The project follows the host's meter and tempo (the chords stay, bar lengths are refitted).
    if (!standalone && ui.hostTempoKnown.load()) {
        const double bpm = ui.bpm.load();
        if (std::fabs(bpm - p->tempo) > 0.005 && bpm >= 1 && bpm <= 999) sess->edit([bpm](Project& q, const Engine&) { q.tempo = bpm; }, false);
        const int num = ui.tsNum.load(), den = ui.tsDen.load();
        if (sess->play().followMeter && den > 0 && (num * 4) % den == 0) {
            const int bpb = num * 4 / den;
            if (bpb >= 1 && bpb <= 16 && bpb != p->bpb) sess->edit([bpb](Project& q, const Engine& e) { e.refitMeter(q, bpb); }, false);
        }
    }
    // A second instance in a link group takes the bass; the first keeps the chords. Only for parts nobody has set by hand.
    if (ticks == 8 && !standalone && !stateRestored && !partTouched && linkId.isNotEmpty()) {
        auto members = sess->members();
        if (members.size() == 2) {
            auto* other = dynamic_cast<KindredProcessor*>(members[0] == this ? members[1] : members[0]);
            if (other && other->partChoice() != 2) {
                setPart(2, false);
                if (other->partChoice() == 0 && !other->partTouched) other->setPart(1, false);
            }
        }
    }
}

void KindredProcessor::togglePreview() {
    const bool on = !sess->play().preview;
    sess->setPlay([on](PlayState& ps) { ps.preview = on; if (on) { ps.previewStartMs = juce::Time::getMillisecondCounterHiRes() + 60.0; ps.previewGen++; } });
}
void KindredProcessor::stopPreview() { if (sess->play().preview) sess->setPlay([](PlayState& ps) { ps.preview = false; }); }

void KindredProcessor::audition(const std::vector<int>& chordNotes, int bassNote) {
    const juce::SpinLock::ScopedLockType l(audLock);
    audPendingCount = 0;
    for (int n : chordNotes) if (audPendingCount < 7) audPending[audPendingCount++] = n;
    audPendingBass = bassNote;
    audSerial++;
}

// ---------------------------------------------------------------- render ----
void KindredProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate > 0 ? sampleRate : 44100;
    pad.prepare(sr);
    panicFlag.store(true);       // anything still sounding from before gets its note-off first
    rebuild();
}
void KindredProcessor::releaseResources() {}
void KindredProcessor::reset() { panicFlag.store(true); }
bool KindredProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    if (!standalone) return l.getMainInputChannelSet().isDisabled() && l.getMainOutputChannelSet().isDisabled();
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

template <class Emit> void KindredProcessor::endAudition(int sample, Emit&& emit) {
    for (int i = 0; i < audCount; i++) emit(sample, (uint8_t) (0x80 | audNotes[i].ch), audNotes[i].pitch, (uint8_t) 0);
    audCount = 0; audRemaining = 0;
}

void KindredProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    buffer.clear();
    auto emit = [&](int sample, uint8_t st, uint8_t d1, uint8_t d2) { const uint8_t m[3] = { st, d1, d2 }; midi.addEvent(m, 3, sample); };
    if (player.isRunning() || player.soundingCount()) player.allOff(0, emit);
    if (audCount) endAudition(0, emit);
    ui.playing.store(false); ui.sounding.store(0); ui.activeLo.store(0); ui.activeHi.store(0);
}

void KindredProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (exLock.tryEnter()) { if (fresh) { std::swap(current, incoming); fresh = false; } exLock.exit(); }
    const RenderData* rd = current.get();

    if (pThru->load() < 0.5f) midi.clear();
    auto emit = [&](int sample, uint8_t st, uint8_t d1, uint8_t d2) { const uint8_t m[3] = { st, d1, d2 }; midi.addEvent(m, 3, juce::jlimit(0, juce::jmax(0, n - 1), sample)); };

    bool hostPlaying = false, hasBpm = false, hasPpq = false; double ppq = 0, bpm = rd ? rd->tempo : 60.0; int num = 4, den = 4;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
        hostPlaying = pos->getIsPlaying();
        if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
        if (auto b = pos->getBpm()) if (*b > 0) { bpm = *b; hasBpm = true; }
        if (auto ts = pos->getTimeSignature()) { num = ts->numerator; den = ts->denominator; }
    }
    if (hostPlaying && !hasPpq) hostPlaying = false;       // a host that plays but gives no position cannot be followed
    if (!(bpm >= 1 && bpm <= 999)) bpm = 60;
    const double bps = bpm / 60.0 / sr;

    if (panicFlag.exchange(false)) { player.allOff(0, emit); endAudition(0, emit); pad.allOff(); }

    bool active = false; double pos = 0, uiPos = 0; bool sectionScope = false;
    if (rd && n > 0) {
        sectionScope = rd->play.sectionScope;
        const Timeline& tl = sectionScope ? rd->section : rd->arrangement;
        const bool loop = sectionScope ? true : rd->play.loop;
        if (hostPlaying) {
            const double beatsPerBar = den > 0 ? num * 4.0 / den : 4.0;
            pos = ppq - (rd->play.startBar - 1) * beatsPerBar;
            if (rd->play.preview) previewEnded.store(true);          // the host has taken over
            active = true;
        } else if (rd->play.preview) {
            if (rd->play.previewGen != lastPreviewGen) {
                lastPreviewGen = rd->play.previewGen;
                previewPos = (juce::Time::getMillisecondCounterHiRes() - rd->play.previewStartMs) * bpm / 60000.0;     // negative until the shared start time
            }
            pos = previewPos; previewPos += n * bps; active = true;
            if (!loop && pos > tl.total + 0.25) previewEnded.store(true);
        }
        if (active) {
            if (audCount) endAudition(0, emit);
            player.process(&tl, pos, bps, n, loop, emit);
            uiPos = loop && tl.total > 0 ? std::fmod(std::max(0.0, pos), tl.total) : pos;
        }
    }
    if (!active) {
        if (player.isRunning() || player.soundingCount()) player.allOff(0, emit);
        // Audition: a chord clicked in the editor sounds for a moment while nothing is playing.
        const int serial = audSerial.load();
        if (serial != audSeen && rd && audLock.tryEnter()) {
            audSeen = serial; endAudition(0, emit);
            const PlayParams& pp = rd->params;
            auto add = [&](int raw, bool bass) {
                const int pitch = raw + 12 * (bass ? pp.bassOct : pp.chordOct);
                if (pitch < 0 || pitch > 127 || audCount >= 8) return;
                const uint8_t ch = (uint8_t) (bass ? pp.bassCh : pp.chordCh);
                emit(0, (uint8_t) (0x90 | ch), (uint8_t) pitch, (uint8_t) (bass ? pp.bassVel : pp.chordVel));
                audNotes[audCount++] = { ch, (uint8_t) pitch };
            };
            if (pp.part != 2) for (int i = 0; i < audPendingCount; i++) add(audPending[i], false);
            if (pp.part != 1 && audPendingBass >= 0) add(audPendingBass, true);
            audRemaining = (long) (1.8 * sr);
            audLock.exit();
        }
        if (audCount) { audRemaining -= n; if (audRemaining <= 0) endAudition(juce::jmax(0, n - 1), emit); }
    }
    wasActive = active;

    // Standalone: the built-in pad plays what goes out.
    if (standalone && buffer.getNumChannels() > 0 && n > 0) {
        const bool on = padOn.load(); const float vol = padVolume.load();
        int at = 0;
        for (const auto meta : midi) {
            const auto m = meta.getMessage(); const int s = juce::jlimit(0, n, meta.samplePosition);
            if (on && s > at) pad.render(buffer, at, s - at, vol);
            at = juce::jmax(at, s);
            const bool bassCh = rd && m.getChannel() - 1 == rd->params.bassCh && rd->params.bassCh != rd->params.chordCh;
            if (m.isNoteOn()) { if (on) pad.noteOn(m.getChannel(), m.getNoteNumber(), m.getVelocity(), bassCh || m.getNoteNumber() < 48); }
            else if (m.isNoteOff()) pad.noteOff(m.getChannel(), m.getNoteNumber());
            else if (m.isAllNotesOff()) pad.allOff();
        }
        if (on && n > at) pad.render(buffer, at, n - at, vol);
        if (!on) pad.allOff();
    }

    uint64_t lo = 0, hi = 0; player.pitchMask(lo, hi);
    for (int i = 0; i < audCount; i++) { const int p = audNotes[i].pitch; if (p < 64) lo |= 1ull << p; else hi |= 1ull << (p - 64); }
    ui.activeLo.store(lo, std::memory_order_relaxed); ui.activeHi.store(hi, std::memory_order_relaxed);
    ui.playing.store(active, std::memory_order_relaxed); ui.hostPlaying.store(hostPlaying, std::memory_order_relaxed);
    ui.sectionScope.store(sectionScope, std::memory_order_relaxed); ui.pos.store(uiPos, std::memory_order_relaxed);
    ui.bpm.store(bpm, std::memory_order_relaxed); ui.hostTempoKnown.store(hasBpm, std::memory_order_relaxed); ui.hostPpq.store(ppq, std::memory_order_relaxed);
    ui.tsNum.store(num, std::memory_order_relaxed); ui.tsDen.store(den, std::memory_order_relaxed);
    ui.sounding.store(player.soundingCount() + audCount, std::memory_order_relaxed);
    ui.blocks.fetch_add(1, std::memory_order_relaxed);
}

// ----------------------------------------------------------------- state ----
void KindredProcessor::getStateInformation(juce::MemoryBlock& dest) {
    std::shared_ptr<Session> s; juce::String link;
    { std::lock_guard<std::mutex> g(latestMx); s = sess; link = linkId; }
    Json j = s->saveState();
    j.set("format", "kindred-state"); j.set("version", 1); j.set("link", link.toStdString());
    Json params = Json::object();
    for (auto* id : kParamIds) if (auto* p = apvts.getParameter(id)) params.set(id, (double) p->convertFrom0to1(p->getValue()));
    j.set("params", params);
    j.set("partTouched", partTouched);
    Json uiState = Json::object(); uiState.set("w", editorW); uiState.set("h", editorH); j.set("ui", uiState);
    Json padState = Json::object(); padState.set("on", padOn.load()); padState.set("volume", (double) padVolume.load()); j.set("pad", padState);
    const std::string text = j.dump();
    dest.replaceAll(text.data(), text.size());
}
void KindredProcessor::setStateInformation(const void* data, int size) {
    if (!data || size <= 0) return;
    Json j;
    try { j = Json::parse(std::string((const char*) data, (size_t) size)); } catch (const std::exception&) { return; }
    if (!j.isObject() || j["format"].stringOr("") != "kindred-state") return;
    stateRestored = true;
    for (auto* id : kParamIds) if (auto* p = apvts.getParameter(id)) if (j["params"][id].isNumber()) {
        settingPartInternally = true;
        p->setValueNotifyingHost(p->convertTo0to1((float) j["params"][id].number()));
        settingPartInternally = false;
    }
    partTouched = j["partTouched"].boolean();
    editorW = j["ui"]["w"].integer(0); editorH = j["ui"]["h"].integer(0);
    if (j["pad"].isObject()) { padOn.store(j["pad"]["on"].boolean(true)); padVolume.store((float) j["pad"]["volume"].number(0.8)); }
    juce::String link = juce::String(j["link"].stringOr(standalone ? "" : "A"));
    if (link.length() > 1) link = link.substring(0, 1);
    if (standalone) link = {};
    joinLink(link, &j);
    paramsDirty.store(false);
    rebuild();
}

std::vector<uint8_t> KindredProcessor::midiFor(const std::string& sectionId, int parts) const {
    std::shared_ptr<Session> s; { std::lock_guard<std::mutex> g(latestMx); s = sess; }
    Project p = *s->project(); const Engine eng = s->engine();
    if (ui.hostTempoKnown.load()) p.tempo = ui.bpm.load();
    std::vector<Event> ev;
    if (sectionId.empty()) ev = eng.arrangementEvents(p);
    else if (const Section* sec = p.section(sectionId)) ev = eng.blockEvents(p, *sec, Engine::sectionBars(p, *sec), 0, nullptr);
    ev = Engine::playable(p, ev);
    if (parts == 2) return eng.midiFileBassOnly(p, ev);
    if (parts == 1) { std::vector<Event> c; for (auto& e : ev) if (!e.bass) c.push_back(e); return eng.midiFile(p, c, false); }
    return eng.midiFile(p, ev, p.bassTrack);
}
juce::File KindredProcessor::writeDragFile(const std::string& sectionId, int parts) const {
    const auto p = sess->project();
    juce::String name = sectionId.empty() ? juce::String("Arrangement") : juce::String::fromUTF8(p->section(sectionId) ? p->section(sectionId)->name.c_str() : "Section");
    name = juce::File::createLegalFileName(name);
    if (parts == 1) name += " (Chords)"; else if (parts == 2) name += " (Bass)";
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(KINDRED_PRODUCT_NAME);
    dir.createDirectory();
    auto file = dir.getChildFile(juce::String(KINDRED_PRODUCT_NAME) + " - " + name + ".mid");
    const auto bytes = midiFor(sectionId, parts);
    file.replaceWithData(bytes.data(), bytes.size());
    return file;
}

juce::AudioProcessorEditor* KindredProcessor::createEditor() { return new KindredEditor(*this); }

}  // namespace kindred

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new kindred::KindredProcessor(); }
