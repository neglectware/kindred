#include "PluginEditor.h"
#include <BinaryData.h>

#if JUCE_MAC
// CoreGraphics: seconds since the last event of one type anywhere in the login session. Declared
// here (it is a plain C function) so that this file stays C++ and needs no system headers.
extern "C" double CGEventSourceSecondsSinceLastEventType(int32_t stateID, uint32_t eventType);
#endif

namespace kindred {

using juce::Colour; using juce::Graphics; using juce::Path; using juce::String;
using Rect = juce::Rectangle<float>;

// ------------------------------------------------------------------ theme ----
Theme Theme::dark() {
    Theme t;
    t.bg = Colour(0xff0b0d12); t.panel = Colour(0xff12151c); t.card = Colour(0xff1a1e28); t.cardHi = Colour(0xff232836); t.line = Colour(0xff262c3a);
    t.text = Colour(0xffe9edf3); t.dim = Colour(0xff8d96a8); t.faint = Colour(0xff596276); t.accent = Colour(0xfff3c152); t.onAccent = Colour(0xff17130a);
    t.playhead = Colour(0xffffffff); t.thread = Colour(0xfff6f1e4);
    t.tonic = Colour(0xfff3c152); t.sub = Colour(0xff48cfbf); t.dom = Colour(0xffff8069); t.med = Colour(0xffa594ff); t.borrowed = Colour(0xffff70b4); t.approach = Colour(0xff8fa3bf);
    return t;
}
Theme Theme::lightTheme() {
    Theme t; t.light = true;
    t.bg = Colour(0xfff3f0ea); t.panel = Colour(0xfffbf9f5); t.card = Colour(0xffffffff); t.cardHi = Colour(0xfff1ede5); t.line = Colour(0xffdcd6ca);
    t.text = Colour(0xff1b1f27); t.dim = Colour(0xff626a78); t.faint = Colour(0xff9a9fa9); t.accent = Colour(0xffd99a13); t.onAccent = Colour(0xff1d1604);
    t.playhead = Colour(0xff1b1f27); t.thread = Colour(0xff2a2f3a);
    t.tonic = Colour(0xffc98a06); t.sub = Colour(0xff0f9f8f); t.dom = Colour(0xffe2553c); t.med = Colour(0xff6b55e0); t.borrowed = Colour(0xffd63a8a); t.approach = Colour(0xff6c7c93);
    return t;
}
Colour Theme::function(char f) const {
    switch (f) { case 'T': return tonic; case 'S': return sub; case 'D': return dom; case 'M': return med; case 'B': return borrowed; default: return approach; }
}
Colour Theme::role(const std::string& r) const {
    if (r == "home") return tonic; if (r == "lift") return sub; if (r == "away") return med; if (r == "turn") return dom;
    return approach;
}

// ------------------------------------------------------------------ icons ----
static Path lockPath(bool closed) {
    Path p;
    p.addRoundedRectangle(0.14f, 0.46f, 0.72f, 0.5f, 0.1f);
    Path sh; sh.startNewSubPath(0.3f, 0.46f); sh.lineTo(0.3f, 0.3f);
    sh.cubicTo(0.3f, 0.04f, 0.7f, 0.04f, 0.7f, 0.3f);
    if (closed) sh.lineTo(0.7f, 0.46f); else sh.lineTo(0.7f, 0.34f);
    juce::PathStrokeType(0.11f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(sh, sh);
    p.addPath(sh);
    return p;
}
static Path make_dicePath() {
    Path p; p.addRoundedRectangle(0.06f, 0.06f, 0.88f, 0.88f, 0.2f);
    Path holes;
    for (auto pt : { juce::Point<float>(0.3f, 0.3f), juce::Point<float>(0.7f, 0.3f), juce::Point<float>(0.5f, 0.5f), juce::Point<float>(0.3f, 0.7f), juce::Point<float>(0.7f, 0.7f) })
        holes.addEllipse(pt.x - 0.075f, pt.y - 0.075f, 0.15f, 0.15f);
    p.setUsingNonZeroWinding(false); p.addPath(holes);
    return p;
}
static Path make_gripPath() { Path p; for (int x = 0; x < 2; x++) for (int y = 0; y < 3; y++) p.addEllipse(0.25f + 0.35f * (float) x, 0.12f + 0.3f * (float) y, 0.16f, 0.16f); return p; }
static Path make_chevronPath() { Path p; p.startNewSubPath(0.2f, 0.36f); p.lineTo(0.5f, 0.66f); p.lineTo(0.8f, 0.36f); juce::PathStrokeType(0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, p); return p; }
static Path arrowPath(bool redo) {
    Path p; p.startNewSubPath(0.22f, 0.42f); p.lineTo(0.62f, 0.42f); p.cubicTo(0.9f, 0.42f, 0.9f, 0.8f, 0.62f, 0.8f); p.lineTo(0.4f, 0.8f);
    juce::PathStrokeType(0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, p);
    Path h; h.addTriangle(0.08f, 0.42f, 0.32f, 0.22f, 0.32f, 0.62f); p.addPath(h);
    if (redo) p.applyTransform(juce::AffineTransform::scale(-1, 1).translated(1, 0));
    return p;
}
static Path make_loopPath() {
    Path p; p.addCentredArc(0.5f, 0.5f, 0.32f, 0.32f, 0, 0.7f, 5.4f, true);
    juce::PathStrokeType(0.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, p);
    Path h; h.addTriangle(0.52f, 0.02f, 0.82f, 0.2f, 0.52f, 0.38f); p.addPath(h);
    return p;
}
static Path make_linkPath() {
    Path p; p.addRoundedRectangle(0.06f, 0.34f, 0.5f, 0.32f, 0.16f); p.addRoundedRectangle(0.44f, 0.34f, 0.5f, 0.32f, 0.16f);
    juce::PathStrokeType(0.09f).createStrokedPath(p, p); return p;
}
static const Path& dicePath() { static const Path p = make_dicePath(); return p; }
static const Path& gripPath() { static const Path p = make_gripPath(); return p; }
static const Path& chevronPath() { static const Path p = make_chevronPath(); return p; }
static const Path& loopPath() { static const Path p = make_loopPath(); return p; }
static const Path& linkPath() { static const Path p = make_linkPath(); return p; }
static void icon(Graphics& g, const Path& unit, Rect r, Colour c) {
    g.setColour(c); g.fillPath(unit, juce::AffineTransform::scale(r.getWidth(), r.getHeight()).translated(r.getX(), r.getY()));
}
// Accidentals are drawn, not typeset: the text face has no flat or sharp.
static Path flatPath() {
    Path p; p.startNewSubPath(0.1f, -1.02f); p.lineTo(0.1f, 0.0f);
    p.startNewSubPath(0.1f, -0.42f); p.cubicTo(0.3f, -0.62f, 0.52f, -0.5f, 0.44f, -0.3f); p.cubicTo(0.38f, -0.16f, 0.2f, -0.06f, 0.1f, 0.0f);
    juce::PathStrokeType(0.085f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, p); return p;
}
static Path sharpPath() {
    Path p; p.startNewSubPath(0.18f, -0.9f); p.lineTo(0.18f, 0.06f); p.startNewSubPath(0.4f, -0.96f); p.lineTo(0.4f, 0.0f);
    juce::PathStrokeType(0.07f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, p);
    Path b; b.startNewSubPath(0.04f, -0.52f); b.lineTo(0.54f, -0.66f); b.startNewSubPath(0.04f, -0.22f); b.lineTo(0.54f, -0.36f);
    juce::PathStrokeType(0.13f, juce::PathStrokeType::curved, juce::PathStrokeType::butt).createStrokedPath(b, b); p.addPath(b); return p;
}

// ------------------------------------------------------------------- text ----
const juce::Font& KindredEditor::realFont(F f) const {
    const int key = f.which * 100000 + juce::roundToInt(f.h * 100);
    auto it = fontCache.find(key);
    if (it == fontCache.end()) {
        auto tf = f.which == 1 ? tfMedium : f.which == 2 ? tfSemi : f.which == 3 ? tfDisplay : f.which == 4 ? tfSerif : tfRegular;
        it = fontCache.emplace(key, juce::Font(juce::FontOptions(tf).withHeight(f.h))).first;
    }
    return it->second;
}
// Text shaping is the slow part of drawing, so each (face, size, string) is shaped once.
const KindredEditor::Run& KindredEditor::run(const String& s, F f) const {
    std::string key = std::to_string(f.which * 100000 + juce::roundToInt(f.h * 100)) + "|" + s.toStdString();
    auto it = runCache.find(key);
    if (it != runCache.end()) return it->second;
    if (runCache.size() > 6000) runCache.clear();
    Run r; const auto& font = realFont(f);
    r.ga.addLineOfText(font, s, 0, 0); r.w = juce::GlyphArrangement::getStringWidth(font, s);
    return runCache.emplace(std::move(key), std::move(r)).first->second;
}
static bool isAccidental(juce::juce_wchar c) { return c == 0x266d || c == 0x266f; }
static bool hasAccidental(const String& s) { for (auto c : s) if (isAccidental(c)) return true; return false; }
float KindredEditor::textWidth(const String& s, F f) const {
    if (!hasAccidental(s)) return run(s, f).w;
    float w = 0; String part; const float cap = realFont(f).getAscent() * 0.74f;
    auto flush = [&] { if (part.isNotEmpty()) { w += run(part, f).w; part.clear(); } };
    for (auto c : s) { if (isAccidental(c)) { flush(); w += cap * (c == 0x266d ? 0.62f : 0.68f); } else part += c; }
    flush(); return w;
}
// Draws one line at a baseline; align 0 left, 1 center, 2 right. Returns the width.
float KindredEditor::text(Graphics& g, const String& s, float x, float baseline, F f, Colour c, int align) const {
    const float w = textWidth(s, f);
    if (align == 1) x -= w / 2; else if (align == 2) x -= w;
    g.setColour(c);
    if (!hasAccidental(s)) { run(s, f).ga.draw(g, juce::AffineTransform::translation(x, baseline)); return w; }
    String part; const float cap = realFont(f).getAscent() * 0.74f;
    auto flush = [&] { if (part.isEmpty()) return; const Run& r = run(part, f); r.ga.draw(g, juce::AffineTransform::translation(x, baseline)); x += r.w; part.clear(); };
    for (auto ch : s) {
        if (!isAccidental(ch)) { part += ch; continue; }
        flush();
        static const Path fl = flatPath(), sh = sharpPath();
        g.fillPath(ch == 0x266d ? fl : sh, juce::AffineTransform::scale(cap).translated(x + cap * 0.04f, baseline));
        x += cap * (ch == 0x266d ? 0.62f : 0.68f);
    }
    flush();
    return w;
}
static String u8(const std::string& s) { return String::fromUTF8(s.c_str()); }
// Root large, extension smaller, slash bass dim. With g == nullptr only measures.
float KindredEditor::chordSymbol(Graphics* g, const Project& p, const Chord& ch, float x, float baseline, float size, Colour main, Colour sub, int align) const {
    const String root = u8(Engine::noteName(ch.root, p.key, mod12(ch.root - p.key), ch.sharp));
    const String ext = u8(ch.ext);
    String bass; if (ch.bass && mod12(*ch.bass) != ch.root) bass = "/" + u8(Engine::noteName(mod12(*ch.bass), p.key, mod12(*ch.bass - p.key), false));
    const auto fr = font(3, size), fe = font(1, size * 0.58f), fb = font(1, size * 0.66f);
    const float wr = textWidth(root, fr), we = ext.isEmpty() ? 0 : textWidth(ext, fe) + size * 0.04f, wb = bass.isEmpty() ? 0 : textWidth(bass, fb) + size * 0.03f;
    const float w = wr + we + wb;
    if (!g) return w;
    if (align == 1) x -= w / 2; else if (align == 2) x -= w;
    text(*g, root, x, baseline, fr, main);
    if (ext.isNotEmpty()) text(*g, ext, x + wr + size * 0.04f, baseline - size * 0.02f, fe, main.withMultipliedAlpha(0.86f));
    if (bass.isNotEmpty()) text(*g, bass, x + wr + we + size * 0.03f, baseline, fb, sub);
    return w;
}

// -------------------------------------------------------- performance panel ----
namespace {
struct PerfPanel : juce::Component {
    struct Row { std::unique_ptr<juce::Label> label; std::unique_ptr<juce::Slider> slider; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att; };
    std::vector<Row> rows; juce::ToggleButton thru{ "MIDI thru (pass incoming notes on)" }, padOn{ "Built-in pad sound" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> thruAtt; juce::Slider padVol; juce::Label padLabel;
    KindredProcessor& proc; juce::LookAndFeel_V4 lf; Colour fill, edge; std::function<void()> onEscape;
    PerfPanel(KindredProcessor& p, const Theme& th) : proc(p), fill(th.light ? th.card : th.cardHi), edge(th.light ? th.line : th.faint.withAlpha(0.7f)) {
        lf.setColourScheme(th.light ? juce::LookAndFeel_V4::getLightColourScheme() : juce::LookAndFeel_V4::getMidnightColourScheme());
        lf.setColour(juce::Slider::thumbColourId, th.accent); lf.setColour(juce::Slider::trackColourId, th.accent.withAlpha(0.6f));
        setLookAndFeel(&lf);
        const std::pair<const char*, const char*> defs[] = { { "chordVel", "Chord velocity" }, { "bassVel", "Bass velocity" }, { "chordOct", "Chord octave" }, { "bassOct", "Bass octave" },
            { "humanize", "Humanize" }, { "strum", "Strum" }, { "gate", "Note length" }, { "chordCh", "Chord MIDI channel" }, { "bassCh", "Bass MIDI channel" } };
        for (auto& d : defs) {
            Row r; r.label = std::make_unique<juce::Label>(juce::String(), d.second); r.slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            r.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
            addAndMakeVisible(*r.label); addAndMakeVisible(*r.slider);
            r.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, d.first, *r.slider);
            rows.push_back(std::move(r));
        }
        addAndMakeVisible(thru); thruAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "thru", thru);
        if (proc.isStandalone()) {
            addAndMakeVisible(padOn); padOn.setToggleState(proc.padOn.load(), juce::dontSendNotification); padOn.onClick = [this] { proc.padOn.store(padOn.getToggleState()); };
            addAndMakeVisible(padLabel); padLabel.setText("Pad volume", juce::dontSendNotification);
            addAndMakeVisible(padVol); padVol.setSliderStyle(juce::Slider::LinearHorizontal); padVol.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            padVol.setRange(0, 1.5, 0.01); padVol.setValue(proc.padVolume.load(), juce::dontSendNotification); padVol.onValueChange = [this] { proc.padVolume.store((float) padVol.getValue()); };
        }
        setSize(380, 16 + 30 * (int) rows.size() + 34 + (proc.isStandalone() ? 64 : 0));
    }
    ~PerfPanel() override { setLookAndFeel(nullptr); }
    void paint(Graphics& g) override {
        g.setColour(fill); g.fillRoundedRectangle(getLocalBounds().toFloat(), 10);
        g.setColour(edge); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10, 1);
    }
    bool keyPressed(const juce::KeyPress& k) override { if (k == juce::KeyPress::escapeKey && onEscape) { auto f = onEscape; f(); return true; } return false; }
    void resized() override {
        int y = 8;
        for (auto& r : rows) { r.label->setBounds(10, y, 140, 26); r.slider->setBounds(150, y, getWidth() - 160, 26); y += 30; }
        thru.setBounds(10, y + 2, getWidth() - 20, 26); y += 34;
        if (proc.isStandalone()) { padOn.setBounds(10, y, getWidth() - 20, 26); y += 30; padLabel.setBounds(10, y, 140, 26); padVol.setBounds(150, y, getWidth() - 160, 26); }
    }
};
}  // namespace

// ----------------------------------------------------------------- editor ----
static const float kRailW = 232, kHeaderH = 56, kMainX = 248, kMainR = 1104, kLaneX = 282, kLaneW = 822;
static const Rect kStage(240, 104, 872, 394), kArr(240, 506, 872, 150);
static const float kCardsY = 146, kCardsH = 98, kLaneY = 254, kLaneH = 170, kBassY = 432, kBassH = 58;
static const int kVoiceLo = 47, kVoiceHi = 82, kBassLo = 31, kBassHi = 47;

KindredEditor::KindredEditor(KindredProcessor& p) : AudioProcessorEditor(p), proc(p) {
    auto load = [](const char* data, int size) { return juce::Typeface::createSystemTypefaceFor(data, (size_t) size); };
    tfRegular = load(BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize); tfMedium = load(BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    tfSemi = load(BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize); tfDisplay = load(BinaryData::InterDisplaySemiBold_ttf, BinaryData::InterDisplaySemiBold_ttfSize);
    tfSerif = load(BinaryData::InstrumentSerifItalic_ttf, BinaryData::InstrumentSerifItalic_ttfSize);
    th = KindredProcessor::settings().getBoolValue("lightTheme", false) ? Theme::lightTheme() : Theme::dark();
    setOpaque(true); setWantsKeyboardFocus(true);
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio((double) baseW / baseH);
    setResizeLimits(840, 540, 2240, 1440);
    const int w = proc.editorW >= 840 ? proc.editorW : KindredProcessor::settings().getIntValue("editorW", baseW);
    setSize(juce::jlimit(840, 2240, w), juce::roundToInt(juce::jlimit(840, 2240, w) * (double) baseH / baseW));
    juce::Desktop::getInstance().addGlobalMouseListener(&pressTap);
    startTimerHz(60);
}
KindredEditor::~KindredEditor() { stopTimer(); juce::Desktop::getInstance().removeGlobalMouseListener(&pressTap); menu.reset(); perf.reset(); if (nameEdit) { nameEdit->onFocusLost = nullptr; nameEdit.reset(); } }
void KindredEditor::visibilityChanged() { if (!isVisible()) closePopups(); }
void KindredEditor::setLightTheme(bool light) { th = light ? Theme::lightTheme() : Theme::dark(); KindredProcessor::settings().setValue("lightTheme", light); repaint(); }
void KindredEditor::resized() {
    const float newScale = (float) getWidth() / (float) baseW;
    if (newScale != scale) { closePopups(); finishRename(true); }                  // a menu is laid out for one size; a resize closes it
    scale = newScale;
    proc.editorW = getWidth(); proc.editorH = getHeight();
    KindredProcessor::settings().setValue("editorW", getWidth());
}
const Section* KindredEditor::selected(const Project& p) const {
    const auto id = proc.session().play().sectionId;
    if (auto* s = p.section(id)) return s;
    return p.sections.empty() ? nullptr : &p.sections.front();
}
void KindredEditor::selectSection(const std::string& id, bool user) {
    if (proc.session().play().sectionId == id) return;
    proc.session().setPlay([&](PlayState& ps) { ps.sectionId = id; });
    selSlot = -1; if (user && proc.ui.playing.load() && !proc.ui.sectionScope.load()) follow = false;
}

// Position inside the selected section while it is the one being played.
double KindredEditor::localPos(const Project& p, bool& inSelected) const {
    inSelected = false;
    if (!proc.ui.playing.load()) return 0;
    const double pos = proc.ui.pos.load(); const Section* sec = selected(p);
    if (!sec || view.total <= 0) return 0;
    if (proc.ui.sectionScope.load()) { inSelected = true; return std::fmod(std::max(0.0, pos), view.total); }
    double start = 0;
    for (auto& b : p.arrangement) {
        const double len = (double) b.bars * p.bpb;
        if (pos >= start && pos < start + len) { if (b.sectionId == sec->id) { inSelected = true; return std::fmod(pos - start, view.total); } return 0; }
        start += len;
    }
    return 0;
}

void KindredEditor::buildView(const Project& p, const Section& sec) {
    view.id = sec.id; view.notes.clear(); view.links.clear(); view.slotStart.clear(); view.total = 0;
    for (auto& sl : sec.slots) { view.slotStart.push_back(view.total); view.total += sl.beats; if (!sl.chord) { view.notes.clear(); return; } }
    if (sec.slots.empty()) return;
    const Engine eng = proc.session().engine();
    const auto ev = Engine::playable(p, eng.blockEvents(p, sec, view.total / p.bpb, 0, nullptr));
    auto slotAt = [&](double t) { int k = 0; for (size_t i = 0; i < view.slotStart.size(); i++) if (t >= view.slotStart[i] - 1e-9) k = (int) i; return k; };
    for (auto& e : ev) for (int n : e.notes) {
        const int slot = slotAt(e.t); const double slotEnd = view.slotStart[(size_t) slot] + sec.slots[(size_t) slot].beats;
        view.notes.push_back({ e.t, e.t + e.len, n, e.bass, slot, e.t + e.len > slotEnd + 1e-9 });
    }
    // Voice leading: where one note ends and another begins at the same moment, pair them from the bottom up.
    std::vector<double> times; for (auto& n : view.notes) if (!n.bass && n.t1 < view.total - 1e-9) times.push_back(n.t1);
    std::sort(times.begin(), times.end()); times.erase(std::unique(times.begin(), times.end(), [](double x, double y) { return std::fabs(x - y) < 1e-9; }), times.end());
    for (double t : times) {
        std::vector<const DNote*> ends, starts;
        for (auto& n : view.notes) { if (n.bass) continue; if (std::fabs(n.t1 - t) < 1e-9) ends.push_back(&n); if (std::fabs(n.t0 - t) < 1e-9) starts.push_back(&n); }
        auto byPitch = [](const DNote* x, const DNote* y) { return x->pitch < y->pitch; };
        std::sort(ends.begin(), ends.end(), byPitch); std::sort(starts.begin(), starts.end(), byPitch);
        if (ends.size() == starts.size()) { for (size_t i = 0; i < ends.size(); i++) if (std::abs(ends[i]->pitch - starts[i]->pitch) <= 5) view.links.push_back({ t, ends[i]->pitch, starts[i]->pitch, ends[i]->slot, starts[i]->slot }); }
        else {
            std::vector<bool> used(starts.size(), false);
            for (auto* e2 : ends) { int best = -1; for (size_t i = 0; i < starts.size(); i++) if (!used[i] && std::abs(starts[i]->pitch - e2->pitch) <= 4 && (best < 0 || std::abs(starts[i]->pitch - e2->pitch) < std::abs(starts[(size_t) best]->pitch - e2->pitch))) best = (int) i;
                if (best >= 0) { used[(size_t) best] = true; view.links.push_back({ t, e2->pitch, starts[(size_t) best]->pitch, e2->slot, starts[(size_t) best]->slot }); } }
        }
    }
}

void KindredEditor::timerCallback() {
    const uint64_t rev = proc.session().revision();
    const auto now = juce::Time::getMillisecondCounter();
    bool full = false;
    if (rev != seenRev) { seenRev = rev; full = true; }
    const bool playing = proc.ui.playing.load(); const double pos = proc.ui.pos.load(); const int sounding = proc.ui.sounding.load();
    if (playing && !wasPlaying) follow = true;
    if (playing && follow && !proc.ui.sectionScope.load()) {
        const auto p = proc.session().project(); double start = 0;
        for (auto& b : p->arrangement) { const double len = (double) b.bars * p->bpb; if (pos >= start && pos < start + len) { selectSection(b.sectionId, false); break; } start += len; }
    }
    if (status.isNotEmpty() && now - statusTime > 5000) { status.clear(); full = true; }
    if (full || playing != wasPlaying) repaint();
    else if (playing && pos != lastPos) { repaint((kStage * scale).getSmallestIntegerContainer()); repaint((kArr * scale).getSmallestIntegerContainer()); repaint(Rect(560, 0, 300, kHeaderH).transformedBy(juce::AffineTransform::scale(scale)).getSmallestIntegerContainer()); }
    else if (sounding != lastSounding || now - flashTime < 500) repaint((kStage * scale).getSmallestIntegerContainer());
    wasPlaying = playing; lastPos = pos; lastSounding = sounding;
    if (nameEdit && nameEditDone) nameEdit.reset();
    if ((menu || perf) && isShowing()) watchOutsidePress();       // only for a window that is on the screen
}

// ------------------------------------------------------------------ paint ----
static void fillRR(Graphics& g, Rect r, float radius, Colour c) { g.setColour(c); g.fillRoundedRectangle(r, radius); }
static void strokeRR(Graphics& g, Rect r, float radius, Colour c, float t = 1) { g.setColour(c); g.drawRoundedRectangle(r.reduced(t / 2), radius, t); }

void KindredEditor::paint(Graphics& g) {
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    g.addTransform(juce::AffineTransform::scale(scale));
    zones.clear();
    const auto p = proc.session().project();
    paintBase(g); paintHeader(g, *p); paintRail(g, *p); paintTabs(g, *p); paintStage(g, *p); paintArrangement(g, *p); paintFooter(g, *p);
    if (perf) juce::DropShadow(juce::Colours::black.withAlpha(th.light ? 0.22f : 0.5f), 22, { 0, 8 }).drawForRectangle(g, perf->getBounds());
    if (menu) paintMenu(g);
    lastPaintMs = juce::Time::getMillisecondCounterHiRes() - t0;
}
void KindredEditor::paintBase(Graphics& g) {
    // The background has two large soft glows, which are slow to fill, so it is kept as an image.
    const float px = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int w = juce::jmax(1, juce::roundToInt((float) baseW * px)), h = juce::jmax(1, juce::roundToInt((float) baseH * px));
    if (baseImage.isNull() || baseImage.getWidth() != w || baseImage.getHeight() != h || baseLight != th.light) {
        baseImage = juce::Image(juce::Image::RGB, w, h, false); baseLight = th.light;
        Graphics b(baseImage); b.addTransform(juce::AffineTransform::scale((float) w / (float) baseW, (float) h / (float) baseH));
        b.fillAll(th.bg);
        if (!th.light) {       // a little light from the top of the stage, so the window is not flat
            b.setGradientFill(juce::ColourGradient(th.accent.withAlpha(0.055f), 620, 60, th.accent.withAlpha(0.0f), 620, 520, true)); b.fillRect(Rect(kRailW, kHeaderH, (float) baseW - kRailW, 480));
            b.setGradientFill(juce::ColourGradient(th.med.withAlpha(0.04f), 1100, 700, th.med.withAlpha(0.0f), 700, 700, true)); b.fillRect(Rect(kRailW, 420, (float) baseW - kRailW, 300));
        }
        b.setColour(th.panel); b.fillRect(Rect(0, 0, (float) baseW, kHeaderH)); b.fillRect(Rect(0, kHeaderH, kRailW, (float) baseH - kHeaderH));
        b.setColour(th.line); b.fillRect(Rect(0, kHeaderH - 1, (float) baseW, 1)); b.fillRect(Rect(kRailW - 1, kHeaderH, 1, (float) baseH - kHeaderH));
        for (auto r : { kStage, kArr }) { b.setColour(th.panel.withAlpha(th.light ? 1.0f : 0.82f)); b.fillRoundedRectangle(r, 14); b.setColour(th.line); b.drawRoundedRectangle(r.reduced(0.5f), 14, 1); }
    }
    g.setOpacity(1.0f);
    g.drawImage(baseImage, Rect(0, 0, (float) baseW, (float) baseH), juce::RectanglePlacement::stretchToFit);
}

void KindredEditor::paintHeader(Graphics& g, const Project& p) {
    const Style& st = *proc.session().currentStyle();
    text(g, KINDRED_PRODUCT_NAME, 22, 38, font(4, 31), th.text);
    auto pill = [&](Rect r, const String& label, const String& value, Kind k, const String& hint, bool chevron = true) {
        const bool hov = hoverKind == k;
        fillRR(g, r, 8, hov ? th.cardHi : th.card); strokeRR(g, r, 8, th.line);
        float x = r.getX() + 11;
        if (label.isNotEmpty()) x += text(g, label, x, r.getCentreY() + 4.2f, font(0, 11.5f), th.dim) + 6;
        text(g, value, x, r.getCentreY() + 4.6f, font(1, 13), th.text);
        if (chevron) icon(g, chevronPath(), Rect(r.getRight() - 20, r.getCentreY() - 6, 12, 12), th.faint);
        zone(r, k, 0, hint);
    };
    pill(Rect(146, 13, 176, 30), "Style", u8(st.name), StylePill, "Style: the kind of music the engine writes. Styles are data files; see the Styles folder in the menu.");
    pill(Rect(330, 13, 76, 30), "Key", u8(Engine::keyName(p.key)), KeyPill, "Key. Applies to the next roll; chords already rolled keep their key.");
    pill(Rect(414, 13, 132, 30), "", u8(st.modeOr(p.mode).name), ModePill, "Mode. Applies to the next roll.");
    const bool hostTempo = !proc.isStandalone() && proc.ui.hostTempoKnown.load();
    pill(Rect(554, 13, 62, 30), "", String(p.bpb) + "/4", MeterPill, proc.session().play().followMeter && !proc.isStandalone() ? "Meter follows the host. Click to set it by hand." : "Meter: beats in a bar.");
    const double bpm = hostTempo ? proc.ui.bpm.load() : p.tempo;
    pill(Rect(624, 13, 84, 30), "", String(bpm, bpm == std::floor(bpm) ? 0 : 1) + " bpm", TempoPill, hostTempo ? "Tempo follows the host." : "Tempo. Click to change, or scroll over it.", !hostTempo);

    // Transport
    const bool playing = proc.ui.playing.load(), host = proc.ui.hostPlaying.load();
    const Rect pb(722, 12, 32, 32);
    g.setColour(playing ? th.accent : (hoverKind == Play ? th.cardHi : th.card)); g.fillEllipse(pb);
    if (!playing) { g.setColour(th.line); g.drawEllipse(pb.reduced(0.5f), 1); }
    Path glyph;
    if (playing && !host) glyph.addRoundedRectangle(pb.getCentreX() - 5, pb.getCentreY() - 5, 10, 10, 1.5f);
    else glyph.addTriangle(pb.getCentreX() - 4, pb.getCentreY() - 6.5f, pb.getCentreX() - 4, pb.getCentreY() + 6.5f, pb.getCentreX() + 7, pb.getCentreY());
    g.setColour(playing ? th.onAccent : th.text); g.fillPath(glyph);
    zone(pb.expanded(3), Play, 0, host ? "The host is playing. Kindred follows its position, tempo and loop." : "Play or stop with Kindred's own clock (Space). When the host plays, Kindred follows the host.");
    {
        const double pos = proc.ui.pos.load();
        String where = "Stopped", sub = host ? "host" : proc.isStandalone() ? "internal clock" : "waiting for host";
        if (playing) { const int bar = (int) std::floor(std::max(0.0, pos) / p.bpb) + 1, beat = (int) std::floor(std::fmod(std::max(0.0, pos), (double) p.bpb)) + 1;
            where = "Bar " + String(bar) + "." + String(beat); sub = host ? "following host" : "preview"; }
        text(g, where, 764, 26, font(2, 13), playing ? th.text : th.dim);
        g.setColour(host ? th.sub : playing ? th.accent : th.faint); g.fillEllipse(764, 33.5f, 6, 6);
        text(g, sub, 775, 41, font(0, 10.5f), th.dim);
    }
    // Part
    const Rect seg(866, 13, 152, 30); fillRR(g, seg, 8, th.card); strokeRR(g, seg, 8, th.line);
    const char* names[] = { "Both", "Chords", "Bass" }; const float sw[] = { 44, 58, 50 }; float sx = seg.getX();
    for (int i = 0; i < 3; i++) {
        const Rect r(sx, seg.getY(), sw[i], 30); const bool on = proc.partChoice() == i;
        if (on) fillRR(g, r.reduced(3), 6, th.accent);
        else if (hoverKind == Part && hoverA == i) fillRR(g, r.reduced(3), 6, th.cardHi);
        text(g, names[i], r.getCentreX(), r.getCentreY() + 4.4f, font(on ? 2 : 1, 12), on ? th.onAccent : th.dim, 1);
        zone(r, Part, i, "What this instance plays. For chords and bass on two tracks: put Kindred on both tracks with the same Link letter, one on Chords and one on Bass.");
        sx += sw[i];
    }
    // Link
    const Rect lk(1024, 13, 46, 30); const bool linked = proc.link().isNotEmpty();
    fillRR(g, lk, 8, hoverKind == LinkPill ? th.cardHi : th.card); strokeRR(g, lk, 8, linked && proc.linkedCount() > 1 ? th.sub : th.line);
    icon(g, linkPath(), Rect(lk.getX() + 6, lk.getCentreY() - 8, 16, 16), linked ? th.sub : th.faint);
    text(g, linked ? proc.link() : String("-"), lk.getRight() - 14, lk.getCentreY() + 4.6f, font(2, 13), linked ? th.text : th.dim, 1);
    zone(lk, LinkPill, 0, linked ? "Link " + proc.link() + ": " + String(proc.linkedCount()) + (proc.linkedCount() == 1 ? " instance shares" : " instances share") + " this progression. Instances with the same letter always show the same music."
                                 : "Not linked: this instance has its own progression.");
    const Rect mb(1078, 13, 26, 30);
    if (hoverKind == MenuBtn) fillRR(g, mb, 8, th.cardHi);
    g.setColour(th.dim); for (int i = 0; i < 3; i++) g.fillEllipse(mb.getCentreX() - 1.8f, mb.getY() + 8 + 5.2f * (float) i, 3.6f, 3.6f);
    zone(mb, MenuBtn, 0, "Menu: projects, MIDI export, performance settings, theme.");
}

void KindredEditor::paintRail(Graphics& g, const Project& p) {
    const Style& st = *proc.session().currentStyle();
    // Roll
    const Rect roll(16, 72, 200, 52); const bool hov = hoverKind == Roll;
    for (int i = 3; i >= 1; i--) { g.setColour(th.accent.withAlpha(th.light ? 0.05f : 0.07f)); g.fillRoundedRectangle(roll.expanded((float) i * 2.0f).translated(0, 2), 12 + (float) i * 2); }
    g.setGradientFill(juce::ColourGradient(th.accent.brighter(hov ? 0.25f : 0.1f), 16, 72, th.accent.darker(0.12f), 16, 124, false)); g.fillRoundedRectangle(roll, 12);
    g.setColour(Colour(0x40ffffff)); g.fillRoundedRectangle(roll.getX() + 12, roll.getY() + 1, roll.getWidth() - 24, 1, 0.5f);
    icon(g, dicePath(), Rect(62, 86, 24, 24), th.onAccent);
    text(g, "Roll", 96, 106, font(3, 21), th.onAccent);
    zone(roll, Roll, 0, "Roll all (R): writes every section again, the verse first. Locked chords stay exactly as they are.");
    auto small = [&](Rect r, const Path& ic, const String& label, Kind k, bool enabled, const String& hint) {
        fillRR(g, r, 7, hoverKind == k && enabled ? th.cardHi : th.card); strokeRR(g, r, 7, th.line);
        const Colour c = enabled ? th.dim : th.faint.withAlpha(0.5f);
        icon(g, ic, Rect(r.getX() + 9, r.getCentreY() - 7, 14, 14), c); text(g, label, r.getX() + 29, r.getCentreY() + 4.2f, font(1, 11.5f), c);
        zone(r, k, 0, hint);
    };
    small(Rect(16, 132, 97, 26), arrowPath(false), "Undo", Undo, proc.session().canUndo(), "Undo the last roll or edit (Cmd-Z).");
    small(Rect(119, 132, 97, 26), arrowPath(true), "Redo", Redo, proc.session().canRedo(), "Redo (Shift-Cmd-Z).");

    text(g, "CHARACTER", 16, 184, font(2, 10), th.faint);
    text(g, "applies to the next roll", 216, 184, font(0, 10), th.faint, 2);
    float y = 194;
    for (size_t i = 0; i < st.spectra.size(); i++) {
        const auto& sp = st.spectra[i]; const double* v = p.mood.byId(sp.id); if (!v) continue;
        const float f = (float) juce::jlimit(0.0, 1.0, (*v - sp.min) / (sp.max - sp.min));
        const bool hv = (hoverKind == SliderZ && hoverA == (int) i) || (dragKind == SliderZ && dragA == (int) i);
        text(g, u8(sp.name), 16, y + 13, font(1, 12), th.text);
        text(g, String(juce::roundToInt(f * 100)), 216, y + 13, font(0, 11), hv ? th.text : th.dim, 2);
        const Rect track(16, y + 22, 200, 5);
        fillRR(g, track, 2.5f, th.card.interpolatedWith(th.line, 0.6f));
        const Colour fillC = sp.id == "edge" ? th.borrowed : th.accent;
        fillRR(g, track.withWidth(std::max(5.0f, 200 * f)), 2.5f, fillC.withAlpha(hv ? 1.0f : 0.85f));
        g.setColour(th.light ? Colour(0xffffffff) : th.text); g.fillEllipse(16 + 200 * f - 7, y + 24.5f - 7, 14, 14);
        g.setColour(fillC); g.drawEllipse(16 + 200 * f - 6.5f, y + 24.5f - 6.5f, 13, 13, hv ? 2.2f : 1.4f);
        text(g, u8(sp.low), 16, y + 46, font(0, 10.5f), th.dim); text(g, u8(sp.high), 216, y + 46, font(0, 10.5f), th.dim, 2);
        zone(Rect(8, y + 2, 216, 50), SliderZ, (int) i, u8(sp.name) + ": " + u8(sp.low) + " (" + u8(sp.lowHint) + ") to " + u8(sp.high) + " (" + u8(sp.highHint) + "). Double-click for the default.");
        y += 56;
    }
    y = 596;
    auto toggle = [&](int idx, const String& label, bool on, const String& hint) {
        const Rect r(16, y, 200, 24); const Rect sw(184, y + 3, 32, 18);
        text(g, label, 16, y + 16, font(1, 12), on ? th.text : th.dim);
        fillRR(g, sw, 9, on ? th.accent : th.card.interpolatedWith(th.line, 0.8f));
        g.setColour(on ? th.onAccent : th.dim); g.fillEllipse(on ? sw.getRight() - 16 : sw.getX() + 2, sw.getY() + 2, 14, 14);
        zone(r, Toggle, idx, hint); y += 28;
    };
    toggle(0, "Hold shared notes", p.tie, "Hold shared notes: a note two chords share is one long note. Turn it off for a piano or any sound that dies away.");
    toggle(1, u8(st.extendedName), p.extended, u8(st.extendedName) + (st.extendedHint.empty() ? String() : " (" + u8(st.extendedHint) + ")") + ". Applies to the next roll.");
    toggle(2, "Major modes only", p.majorModesOnly, "Major modes only: a roll never uses a mode with a minor tonic chord.");
}

void KindredEditor::paintTabs(Graphics& g, const Project& p) {
    const Section* sel = selected(p); float x = kMainX;
    for (size_t i = 0; i < p.sections.size(); i++) {
        const auto& s = p.sections[i]; const bool on = sel && s.id == sel->id; const auto f = font(on ? 2 : 1, 13);
        const float w = textWidth(u8(s.name), f) + 30; const Rect r(x, 64, w, 32);
        if (x + w > kMainR - 40) break;
        if (on) { fillRR(g, r, 8, th.card); g.setColour(th.role(s.role)); g.fillRoundedRectangle(r.getX() + 12, r.getBottom() - 3, w - 24, 2.5f, 1.2f); }
        else if (hoverKind == Tab && hoverA == (int) i) fillRR(g, r, 8, th.panel);
        g.setColour(th.role(s.role).withAlpha(on ? 1.0f : 0.6f)); g.fillEllipse(r.getX() + 10, r.getCentreY() - 3, 6, 6);
        text(g, u8(s.name), r.getX() + 21, r.getCentreY() + 4.6f, f, on ? th.text : th.dim);
        zone(r, Tab, (int) i, "Section. Click to show it, double-click to rename, right-click for more.");
        x += w + 4;
    }
    const Rect add(x + 2, 66, 28, 28);
    if (hoverKind == TabAdd) fillRR(g, add, 8, th.card);
    g.setColour(th.dim); g.fillRect(Rect(add.getCentreX() - 5, add.getCentreY() - 0.75f, 10, 1.5f)); g.fillRect(Rect(add.getCentreX() - 0.75f, add.getCentreY() - 5, 1.5f, 10));
    zone(add, TabAdd, 0, "Add a section.");
}

void KindredEditor::paintStage(Graphics& g, const Project& p) {
    const Style& st = *proc.session().currentStyle();
    const Section* sec = selected(p);
    if (!sec) { text(g, "No sections. Add one with +.", kStage.getCentreX(), kStage.getCentreY(), font(1, 14), th.dim, 1); return; }
    if (view.id != sec->id || view.rev != seenRev) { buildView(p, *sec); view.rev = seenRev; }

    // Section settings row
    auto pill = [&](Rect r, const String& label, const String& value, Kind k, const String& hint) {
        fillRR(g, r, 7, hoverKind == k ? th.cardHi : th.card);
        float x = r.getX() + 9; if (label.isNotEmpty()) x += text(g, label, x, r.getCentreY() + 4, font(0, 11), th.dim) + 5;
        g.saveState(); g.reduceClipRegion(r.withTrimmedRight(18).toNearestInt()); text(g, value, x, r.getCentreY() + 4.2f, font(1, 12), th.text); g.restoreState();
        icon(g, chevronPath(), Rect(r.getRight() - 17, r.getCentreY() - 5.5f, 11, 11), th.faint);
        zone(r, k, 0, hint);
    };
    const float ry = 113;
    pill(Rect(250, ry, 128, 24), "Role", u8(st.roleName(sec->role)), SecRole, "Role: how this section relates to the verse. Roll the section to apply a new role.");
    pill(Rect(384, ry, 88, 24), "", String(sec->length == 4 ? 4 : 8) + " chords", SecLen, "Chords in this section: 4 or 8.");
    pill(Rect(478, ry, 98, 24), "", String(sec->chordBars) + (sec->chordBars == 1 ? " bar each" : " bars each"), SecBars, "Bars per chord.");
    String planText = sec->planPref == "auto" ? "Auto" : String();
    if (sec->plan) planText = (sec->planPref == "auto" ? String("Auto: ") : String()) + u8(sec->plan->label);
    pill(Rect(582, ry, 268, 24), "Bass", planText, SecPlan, "Bass plan: where the bass holds a drone and where it follows the chords. Auto lets the roll choose.");
    {
        const Rect r(858, ry, 104, 24); const bool hv = hoverKind == SecRoll;
        fillRR(g, r, 7, hv ? th.accent.withAlpha(0.22f) : th.card); strokeRR(g, r, 7, th.accent.withAlpha(hv ? 0.9f : 0.45f));
        icon(g, dicePath(), Rect(r.getX() + 8, r.getCentreY() - 7, 14, 14), th.accent); text(g, "Roll section", r.getX() + 27, r.getCentreY() + 4.2f, font(1, 12), th.text);
        zone(r, SecRoll, 0, "Roll this section only. Locked chords stay.");
        bool all = !sec->slots.empty(); for (auto& sl : sec->slots) if (!sl.locked) all = false;
        const Rect lr(968, ry, 28, 24); fillRR(g, lr, 7, hoverKind == SecLock ? th.cardHi : th.card);
        icon(g, lockPath(all), Rect(lr.getCentreX() - 7, lr.getCentreY() - 7.5f, 14, 14), all ? th.accent : th.dim);
        zone(lr, SecLock, 0, all ? "Unlock every chord in this section." : "Lock every chord in this section.");
        const bool scope = proc.session().play().sectionScope; const Rect sr(1002, ry, 102, 24);
        fillRR(g, sr, 7, scope ? th.sub.withAlpha(0.2f) : (hoverKind == Scope ? th.cardHi : th.card)); if (scope) strokeRR(g, sr, 7, th.sub.withAlpha(0.8f));
        icon(g, loopPath(), Rect(sr.getX() + 7, sr.getCentreY() - 7, 14, 14), scope ? th.sub : th.dim); text(g, "Loop section", sr.getX() + 25, sr.getCentreY() + 4.2f, font(1, 12), scope ? th.text : th.dim);
        zone(sr, Scope, 0, "Loop section: play only this section, again and again, instead of the arrangement.");
    }
    if (view.total <= 0 || view.notes.empty()) { text(g, "This section is empty. Press Roll section.", kStage.getCentreX(), 300, font(1, 14), th.dim, 1); return; }

    const auto now = juce::Time::getMillisecondCounter();
    bool playingHere = false; const double lp = localPos(p, playingHere);
    auto xAt = [&](double beat) { return kLaneX + (float) (beat / view.total) * kLaneW; };
    int playSlot = -1;
    if (playingHere) for (size_t i = 0; i < view.slotStart.size(); i++) if (lp >= view.slotStart[i]) playSlot = (int) i;

    // Lanes background
    const Rect lane(kLaneX, kLaneY, kLaneW, kLaneH), bassLane(kLaneX, kBassY, kLaneW, kBassH);
    fillRR(g, Rect(250, kLaneY, kMainR - 250, kLaneH), 8, th.bg.interpolatedWith(th.panel, 0.35f)); fillRR(g, Rect(250, kBassY, kMainR - 250, kBassH), 8, th.bg.interpolatedWith(th.panel, 0.35f));
    const float semi = kLaneH / (float) (kVoiceHi - kVoiceLo + 1), bsemi = kBassH / (float) (kBassHi - kBassLo + 1);
    auto yOf = [&](int pitch) { return kLaneY + kLaneH - ((float) (pitch - kVoiceLo) + 0.5f) * semi; };
    auto yBass = [&](int pitch) { return kBassY + kBassH - ((float) (pitch - kBassLo) + 0.5f) * bsemi; };
    // Keyboard strip: the pitch axis. Sounding keys light up.
    std::vector<bool> lit(128, false);
    if (playingHere) for (auto& n : view.notes) if (lp >= n.t0 && lp < n.t1) lit[(size_t) juce::jlimit(0, 127, n.pitch)] = true;
    if (!playingHere && selSlot >= 0 && selSlot < (int) sec->slots.size() && sec->slots[(size_t) selSlot].chord && proc.ui.sounding.load() > 0) {
        for (int n : sec->slots[(size_t) selSlot].chord->notes) lit[(size_t) n] = true;
        if (sec->slots[(size_t) selSlot].chord->bass) lit[(size_t) *sec->slots[(size_t) selSlot].chord->bass] = true;
    }
    auto keys = [&](int lo, int hi, float y0, float h, float s, std::function<float(int)> yf) {
        g.saveState(); Path clip; clip.addRoundedRectangle(250, y0, 28, h, 8, 8, true, false, true, false); g.reduceClipRegion(clip);
        g.setColour(th.light ? Colour(0xffffffff) : Colour(0xffd9dde6)); g.fillRect(Rect(250, y0, 28, h));
        for (int pch = lo; pch <= hi; pch++) {
            const int pc = pch % 12; const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; const float y = yf(pch) - s / 2;
            if (black) { g.setColour(Colour(0xff171a22)); g.fillRect(Rect(250, y, 17, s)); }
            else if (pc == 0 || pc == 5) { g.setColour(Colour(0x30000000)); g.fillRect(Rect(250, y + s - 0.5f, 28, 0.6f)); }
            if (lit[(size_t) pch]) { g.setColour(th.accent); g.fillRect(Rect(250, y, black ? 17.0f : 28.0f, s)); }
        }
        g.restoreState();
    };
    keys(kVoiceLo, kVoiceHi, kLaneY, kLaneH, semi, yOf); keys(kBassLo, kBassHi, kBassY, kBassH, bsemi, yBass);
    // Octave lines and slot separators
    for (int pch = kVoiceLo; pch <= kVoiceHi; pch++) if (pch % 12 == 0) { g.setColour(th.line.withAlpha(0.7f)); g.fillRect(Rect(kLaneX, yOf(pch) + semi / 2, kLaneW, 1)); text(g, "C" + String(pch / 12 - 1), kMainR - 6, yOf(pch) + semi / 2 - 3, font(0, 9), th.faint, 2); }
    text(g, "BASS", kMainR - 6, kBassY + 12, font(2, 9), th.faint, 2);
    for (size_t i = 1; i < view.slotStart.size(); i++) { g.setColour(th.line.withAlpha(0.55f)); const float x = xAt(view.slotStart[i]); g.fillRect(Rect(x - 0.5f, kLaneY + 4, 1, kLaneH - 8)); g.fillRect(Rect(x - 0.5f, kBassY + 4, 1, kBassH - 8)); }

    // Cards
    if (lastCardText.size() != sec->slots.size() || cardFlash.size() != sec->slots.size()) { lastCardText.assign(sec->slots.size(), {}); cardFlash.assign(sec->slots.size(), 0); }
    std::vector<Colour> slotColour;
    for (size_t i = 0; i < sec->slots.size(); i++) {
        const Slot& sl = sec->slots[i]; const Chord& ch = *sl.chord;
        const Colour fc = th.function(Engine::functionOf(p, ch)); slotColour.push_back(fc);
        const float x0 = xAt(view.slotStart[i]), x1 = xAt(view.slotStart[i] + sl.beats);
        const Rect r(x0 + 3, kCardsY, x1 - x0 - 6, kCardsH);
        const std::string sig = sec->id + Engine::chordName(ch, p.key) + (sl.sus ? "s" : "") + (sl.tail ? "t" : "") + (sl.leadIn ? "l" : "");
        if (lastCardText[i] != sig) { if (!lastCardText[i].empty()) { cardFlash[i] = now; flashTime = now; } lastCardText[i] = sig; }
        const bool hov = (hoverKind == Card || hoverKind == CardLock || hoverKind == CardDice) && hoverA == (int) i;
        const bool isPlaying = playSlot == (int) i;
        Colour bgc = sl.locked ? th.cardHi : th.card; if (hov) bgc = bgc.brighter(th.light ? 0.0f : 0.06f).interpolatedWith(fc, 0.05f);
        if (isPlaying) bgc = bgc.interpolatedWith(fc, th.light ? 0.16f : 0.2f);
        if (isPlaying) for (int k = 3; k >= 1; k--) { g.setColour(fc.withAlpha(0.06f)); g.fillRoundedRectangle(r.expanded((float) k * 2.5f), 10 + (float) k * 2.5f); }
        g.setGradientFill(juce::ColourGradient(bgc.brighter(th.light ? 0.0f : 0.05f), r.getX(), r.getY(), bgc, r.getX(), r.getBottom(), false)); g.fillRoundedRectangle(r, 10);
        g.setColour(fc.withAlpha(isPlaying ? 1.0f : 0.85f)); g.fillRoundedRectangle(r.getX() + 10, r.getY(), r.getWidth() - 20, 2.5f, 1.2f);
        if (sl.locked) strokeRR(g, r, 10, th.accent.withAlpha(0.95f), 1.6f);
        else if (isPlaying) strokeRR(g, r, 10, fc.withAlpha(0.8f), 1.2f);
        else if ((int) i == selSlot) strokeRR(g, r, 10, th.dim.withAlpha(0.6f), 1);
        const float age = (float) (now - cardFlash[i]);
        if (cardFlash[i] && age < 420) { g.setColour(fc.withAlpha(0.35f * (1 - age / 420))); g.fillRoundedRectangle(r, 10); }
        // bar number
        const int bar = (int) std::lround(view.slotStart[i] / p.bpb) + 1;
        text(g, String(bar), r.getX() + 9, r.getY() + 18, font(1, 10.5f), th.faint);
        // lock
        const Rect lr(r.getRight() - 24, r.getY() + 6, 18, 18);
        if (sl.locked || hov) icon(g, lockPath(sl.locked), lr.reduced(2), sl.locked ? th.accent : (hoverKind == CardLock && hov ? th.text : th.dim));
        // chord symbol, fitted to the card
        float size = 25; const float avail = r.getWidth() - 14;
        while (size > 12 && chordSymbol(nullptr, p, ch, 0, 0, size, th.text, th.dim, 0) > avail) size -= 1;
        chordSymbol(&g, p, ch, r.getCentreX(), r.getY() + 50, size, th.text, th.dim, 1);
        String roman = u8(Engine::romanName(ch, p.key)); if (ch.kind == "borrowed") roman += "  borrowed";
        auto rf = font(1, 11.5f); if (textWidth(roman, rf) > avail) roman = u8(Engine::romanName(ch, p.key));
        text(g, roman, r.getCentreX(), r.getY() + 68, rf, fc, 1);
        String orn;
        if (sl.sus) orn = u8(Engine::pcName(sl.sus->from, p.key)) + String::fromUTF8(" \xe2\x86\x92 ") + u8(Engine::pcName(sl.sus->to, p.key)) + " held";
        else if (sl.tail) orn = String::fromUTF8("\xe2\x86\x92 ") + u8(Engine::pcName(sl.tail->to, p.key)) + " passing";
        if (i + 1 < sec->slots.size() && sec->slots[i + 1].leadIn) orn = (orn.isEmpty() ? String() : orn + "  ") + String::fromUTF8("\xe2\x80\xb9") + u8(Engine::chordName(*sec->slots[i + 1].leadIn, p.key)) + String::fromUTF8("\xe2\x80\xba");
        auto of = font(0, 10); if (textWidth(orn, of) > avail) orn = orn.upToFirstOccurrenceOf(" held", false, false).upToFirstOccurrenceOf(" passing", false, false);
        zone(r, Card, (int) i, sl.locked ? "Locked: no roll can change this chord, its bass or its held notes. Click the lock to free it."
                                         : "Click to hear this chord. The lock keeps it through every roll; the dice swaps only this chord for a related one.");
        if (!sl.locked) {
            const Rect dr(r.getRight() - 24, r.getBottom() - 24, 18, 18);
            if (hov) icon(g, dicePath(), dr.reduced(1.5f), hoverKind == CardDice ? th.accent : th.dim);
            zone(dr.expanded(3), CardDice, (int) i, "Re-roll this chord only.");
        }
        if (hov && !sl.locked) text(g, orn, r.getX() + 9, r.getBottom() - 10, of, th.dim); else text(g, orn, r.getCentreX(), r.getBottom() - 10, of, th.dim, 1);
        zone(lr.expanded(3), CardLock, (int) i, sl.locked ? "Unlock this chord." : "Lock this chord.");
    }

    // Voice leading: a thin curve from each note that ends to the note that voice moves to.
    for (auto& l : view.links) {
        const float x = xAt(l.t), y0 = yOf(juce::jlimit(kVoiceLo, kVoiceHi, l.from)), y1 = yOf(juce::jlimit(kVoiceLo, kVoiceHi, l.to));
        Path c; c.startNewSubPath(x - 3.5f, y0); c.cubicTo(x + 1.0f, y0, x - 1.0f, y1, x + 3.5f, y1);
        const Colour a = slotColour[(size_t) juce::jlimit(0, (int) slotColour.size() - 1, l.slotFrom)], b2 = slotColour[(size_t) juce::jlimit(0, (int) slotColour.size() - 1, l.slotTo)];
        g.setColour(a.interpolatedWith(b2, 0.5f).withAlpha(0.6f));
        g.strokePath(c, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // Notes. A note held across a chord change is drawn as one unbroken thread.
    for (int pass = 0; pass < 2; pass++) for (auto& n : view.notes) {
        const bool active = playingHere && lp >= n.t0 && lp < n.t1;
        if ((pass == 1) != active) continue;
        const float x0 = xAt(n.t0) + 2.5f, x1 = xAt(n.t1) - 2.5f; if (x1 <= x0) continue;
        const Colour base = n.shared && !n.bass ? th.thread : slotColour[(size_t) juce::jlimit(0, (int) slotColour.size() - 1, n.slot)];
        if (n.bass) {
            const float y = yBass(juce::jlimit(kBassLo, kBassHi, n.pitch)); const Rect r(x0, y - 4.5f, x1 - x0, 9);
            const bool drone = n.shared;
            if (active) { g.setColour(th.accent.withAlpha(0.22f)); g.fillRoundedRectangle(r.expanded(3), 7); }
            fillRR(g, r, 4.5f, drone ? th.accent.withAlpha(active ? 1.0f : 0.8f) : th.dim.withAlpha(active ? 1.0f : 0.75f));
            if (r.getWidth() > 60) text(g, u8(Engine::pcName(n.pitch, p.key)) + (drone ? " drone" : ""), r.getX() + 7, y + 3.2f, font(2, 8.5f), drone ? th.onAccent : th.bg);
        } else {
            const float y = yOf(juce::jlimit(kVoiceLo, kVoiceHi, n.pitch)); const float h = active ? 5.6f : 4.6f; const Rect r(x0, y - h / 2, x1 - x0, h);
            if (active) { g.setColour(base.withAlpha(0.28f)); g.fillRoundedRectangle(r.expanded(3, 3.5f), 6); }
            fillRR(g, r, h / 2, base.withAlpha(active ? 1.0f : (n.shared ? 0.9f : 0.78f)));
        }
    }
    zone(Rect(250, kLaneY, 28, kLaneH + kBassH + 8), Keyboard, 0, "Pitch: upper voices above, bass below. A note two chords share is one unbroken line.");
    // Playhead
    if (playingHere) {
        const float x = xAt(lp);
        g.setGradientFill(juce::ColourGradient(th.playhead.withAlpha(0.0f), x - 26, 0, th.playhead.withAlpha(th.light ? 0.10f : 0.13f), x, 0, false));
        g.fillRect(Rect(x - 26, kCardsY, 26, kBassY + kBassH - kCardsY));
        g.setColour(th.playhead.withAlpha(0.9f)); g.fillRect(Rect(x - 0.75f, kCardsY - 3, 1.5f, kBassY + kBassH - kCardsY + 6));
    }
}

void KindredEditor::paintArrangement(Graphics& g, const Project& p) {
    double totalBars = 0; for (auto& b : p.arrangement) totalBars += b.bars;
    const double bpm = !proc.isStandalone() && proc.ui.hostTempoKnown.load() ? proc.ui.bpm.load() : p.tempo;
    const int secs = (int) std::lround(totalBars * p.bpb * 60.0 / std::max(1.0, bpm));
    float tx = 250; tx += text(g, "ARRANGEMENT", tx, 527, font(2, 10), th.faint) + 10;
    text(g, String((int) totalBars) + " bars  " + String::fromUTF8("\xc2\xb7") + "  " + String(secs / 60) + ":" + String(secs % 60).paddedLeft('0', 2), tx, 527, font(0, 11), th.dim);
    {
        const bool loop = proc.session().play().loop; const Rect r(946, 513, 70, 22);
        fillRR(g, r, 7, loop ? th.sub.withAlpha(0.2f) : (hoverKind == LoopToggle ? th.cardHi : th.card));
        icon(g, loopPath(), Rect(r.getX() + 7, r.getCentreY() - 6.5f, 13, 13), loop ? th.sub : th.dim); text(g, "Repeat", r.getX() + 24, r.getCentreY() + 4, font(1, 11.5f), loop ? th.text : th.dim);
        zone(r, LoopToggle, 0, "Repeat: when the arrangement ends, start it again.");
        const Rect a(1022, 513, 82, 22); fillRR(g, a, 7, hoverKind == BlockAdd ? th.cardHi : th.card);
        text(g, "+ Add block", a.getCentreX(), a.getCentreY() + 4, font(1, 11.5f), th.dim, 1);
        zone(a, BlockAdd, 0, "Add a block of the section that is showing.");
    }
    const float X = 250, W = kMainR - 250, Y = 542, H = 82;
    if (totalBars <= 0) { text(g, "No blocks. Add one to build a song from the sections.", X + W / 2, Y + 40, font(1, 13), th.dim, 1); return; }
    const Section* sel = selected(p);
    double bar = 0; int playBlock = -1; const bool arrPlaying = proc.ui.playing.load() && !proc.ui.sectionScope.load(); const double pos = proc.ui.pos.load();
    for (size_t i = 0; i < p.arrangement.size(); i++) {
        const auto& b = p.arrangement[i]; const Section* s = p.section(b.sectionId);
        const float x0 = X + (float) (bar / totalBars) * W, x1 = X + (float) ((bar + b.bars) / totalBars) * W; const Rect r(x0 + 1.5f, Y, x1 - x0 - 3, H);
        const Colour rc = s ? th.role(s->role) : th.faint; const bool isSel = s && sel && s->id == sel->id;
        const bool here = arrPlaying && pos >= bar * p.bpb && pos < (bar + b.bars) * p.bpb; if (here) playBlock = (int) i;
        const bool hov = (hoverKind == BlockZ || hoverKind == BlockEdge) && hoverA == (int) i;
        fillRR(g, r, 8, th.card.interpolatedWith(rc, here ? 0.34f : isSel ? 0.22f : 0.11f).brighter(hov && !th.light ? 0.05f : 0));
        if (isSel) strokeRR(g, r, 8, rc.withAlpha(0.75f), 1.2f);
        if (dragKind == BlockZ && dragA == (int) i && dragMoved) strokeRR(g, r, 8, th.text, 1.5f);
        g.saveState(); g.reduceClipRegion(r.reduced(6, 0).toNearestInt());
        text(g, s ? u8(s->name) : String("?"), r.getX() + 9, r.getY() + 20, font(2, 12), th.text);
        text(g, String(b.bars) + (b.bars == 1 ? " bar" : " bars"), r.getX() + 9, r.getY() + 35, font(0, 10.5f), th.dim);
        if (s && r.getWidth() > 70) {        // the chords, small, once through
            String line; for (size_t k = 0; k < s->slots.size() && k < 8; k++) if (s->slots[k].chord) line += (k ? "  " : "") + u8(Engine::chordName(*s->slots[k].chord, p.key));
            text(g, line, r.getX() + 9, r.getBottom() - 10, font(0, 9.5f), rc.interpolatedWith(th.dim, 0.35f));
        }
        g.restoreState();
        g.setColour(rc.withAlpha(hov ? 0.9f : 0.35f)); g.fillRoundedRectangle(r.getRight() - 5, r.getY() + 20, 2, H - 40, 1);
        zone(r, BlockZ, (int) i, "Block: plays a section for a number of bars (a longer block loops the section). Drag to reorder, drag the right edge to change its length, right-click for more.");
        zone(Rect(r.getRight() - 9, r.getY(), 11, H), BlockEdge, (int) i, "Drag to change the number of bars.");
        bar += b.bars;
    }
    if (dragKind == BlockZ && dragMoved && dropIndex >= 0) {
        double bb = 0; for (int i = 0; i < dropIndex && i < (int) p.arrangement.size(); i++) bb += p.arrangement[(size_t) i].bars;
        g.setColour(th.accent); g.fillRoundedRectangle(X + (float) (bb / totalBars) * W - 1.5f, Y - 3, 3, H + 6, 1.5f);
    }
    // ruler
    const int step = totalBars > 96 ? 16 : totalBars > 40 ? 8 : 4;
    for (int b = 0; b <= (int) totalBars; b += step) { const float x = X + (float) (b / totalBars) * W; g.setColour(th.line); g.fillRect(Rect(x, Y + H + 5, 1, 5)); if (b < (int) totalBars) text(g, String(b + 1), x + 4, Y + H + 14, font(0, 9.5f), th.faint); }
    if (arrPlaying && pos >= 0 && pos <= totalBars * p.bpb) {
        const float x = X + (float) (pos / (totalBars * p.bpb)) * W;
        g.setColour(th.playhead.withAlpha(0.9f)); g.fillRect(Rect(x - 0.75f, Y - 4, 1.5f, H + 16));
        Path tri; tri.addTriangle(x - 4.5f, Y - 8, x + 4.5f, Y - 8, x, Y - 1); g.fillPath(tri);
    }
    (void) playBlock;
}

void KindredEditor::paintFooter(Graphics& g, const Project& p) {
    const Section* sec = selected(p);
    String msg = hoverHint.isNotEmpty() ? hoverHint : status;
    if (msg.isEmpty()) {
        msg = "Roll, lock what you like, roll again. Drag a section or the arrangement to a track when it is right.";
        if (!proc.styleProblems.isEmpty()) msg = "Style file problem: " + proc.styleProblems[0];
    }
    // Two lines at most.
    const auto f = font(0, 12); juce::StringArray lines; String line;
    for (auto& w : juce::StringArray::fromTokens(msg, " ", "")) { if (textWidth(line + " " + w, f) > 492 && line.isNotEmpty()) { lines.add(line); line = w; } else line = line.isEmpty() ? w : line + " " + w; }
    lines.add(line);
    for (int i = 0; i < juce::jmin(2, lines.size()); i++) text(g, lines[i], kMainX, (lines.size() > 1 ? 690.0f : 698.0f) + 16.0f * (float) i, f, hoverHint.isNotEmpty() || status.isEmpty() ? th.dim : th.text);
    auto tile = [&](Rect r, const String& title, const String& sub, Kind k, const String& hint) {
        const bool hov = hoverKind == k;
        fillRR(g, r, 10, hov ? th.cardHi : th.card); strokeRR(g, r, 10, hov ? th.accent.withAlpha(0.8f) : th.line);
        icon(g, gripPath(), Rect(r.getX() + 9, r.getCentreY() - 9, 13, 18), hov ? th.accent : th.faint);
        text(g, title, r.getX() + 29, r.getY() + 17, font(2, 12), th.text);
        g.saveState(); g.reduceClipRegion(r.reduced(4, 0).toNearestInt()); text(g, sub, r.getX() + 29, r.getY() + 31, font(0, 10), th.dim); g.restoreState();
        zone(r, k, 0, hint);
    };
    const char* parts[] = { "chords + bass", "chords", "bass" };
    tile(Rect(760, 672, 168, 40), "Drag section", (sec ? u8(sec->name) : String()) + String::fromUTF8(" \xc2\xb7 ") + parts[proc.partChoice()], DragSection,
         "Drag from here to a track in your DAW: the section as a MIDI region. With Chords + Bass, the bass is a second track on channel 2.");
    tile(Rect(936, 672, 168, 40), "Drag arrangement", String("whole song") + String::fromUTF8(" \xc2\xb7 ") + parts[proc.partChoice()], DragArr,
         "Drag from here to a track in your DAW: the whole arrangement as a MIDI region.");
}

// ------------------------------------------------------------------ input ----
const KindredEditor::Zone* KindredEditor::zoneAt(juce::Point<float> p) const {
    for (auto it = zones.rbegin(); it != zones.rend(); ++it) if (it->r.contains(p)) return &*it;
    return nullptr;
}
void KindredEditor::updateHover() {
    const Zone* z = menu || perf ? nullptr : zoneAt(hoverPt);       // nothing under an open menu reacts
    const Kind k = z ? z->kind : None; const int a = z ? z->a : -1; const String h = z ? z->hint : String();
    if (k != hoverKind || a != hoverA || h != hoverHint) { hoverKind = k; hoverA = a; hoverHint = h; repaint(); }
    setMouseCursor(k == BlockEdge ? juce::MouseCursor::LeftRightResizeCursor : (k == DragSection || k == DragArr) ? juce::MouseCursor::DraggingHandCursor
                   : (k != None && k != Keyboard) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void KindredEditor::mouseMove(const juce::MouseEvent& e) { hoverPt = toBase(e.position); if (menu) menuHover(hoverPt); updateHover(); }
void KindredEditor::mouseExit(const juce::MouseEvent&) { hoverPt = { -1, -1 }; if (menu) menuHover(hoverPt); updateHover(); }

// ------------------------------------------------------------------ menus ----
// Every menu is drawn here, inside the editor, from a juce::PopupMenu used only as a list of
// items. It is not a window and it is not modal. Why: JUCE's own popup is a separate top-level
// window that closes itself by watching the key window and the keyboard focus, and inside a host
// both belong to the host. See NOTES.md, "The menu fault".
static const float kMenuRowH = 25, kMenuSepH = 9, kMenuPad = 5, kMenuGutter = 27;

void KindredEditor::showMenu(juce::PopupMenu& m, Rect r, std::function<void(int)> fn) {
    closePopups();
    auto st = std::make_unique<MenuState>(); st->fn = std::move(fn);
    MenuPanel root; { juce::PopupMenu::MenuItemIterator it(m); while (it.next()) { const auto& i = it.getItem(); MenuEntry e; e.id = i.itemID; e.text = i.text; e.enabled = i.isEnabled; e.ticked = i.isTicked; e.separator = i.isSeparator;
        if (i.subMenu) { juce::PopupMenu::MenuItemIterator it2(*i.subMenu); while (it2.next()) { const auto& j = it2.getItem(); MenuEntry c; c.id = j.itemID; c.text = j.text; c.enabled = j.isEnabled; c.ticked = j.isTicked; c.separator = j.isSeparator; e.sub.push_back(c); } }
        root.items.push_back(e); } }
    while (!root.items.empty() && root.items.back().separator) root.items.pop_back();
    if (root.items.empty()) return;
    layoutMenuPanel(root, r, false);
    st->panels.push_back(std::move(root));
    st->openedAt = juce::Time::getMillisecondCounter(); st->openerPress = true; st->openerPt = hoverPt;
    menu = std::move(st); menusOpened++;
    popupOpenedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0; popupSawRelease = false; popupDownSeenAt = -1;
    updateHover(); repaint();
}
// Rows in one column, or in several when the list is taller than the window. Below the anchor,
// or above it when there is no room; a submenu goes to the right of its row, or to the left.
void KindredEditor::layoutMenuPanel(MenuPanel& pn, Rect anchor, bool asSubmenu) const {
    const auto f = font(1, 12.5f); float textW = 0; bool anySub = false;
    for (auto& e : pn.items) if (!e.separator) { textW = std::max(textW, textWidth(e.text, f)); anySub = anySub || !e.sub.empty(); }
    const float colW = std::max(asSubmenu ? 96.0f : std::max(120.0f, anchor.getWidth()), kMenuGutter + textW + 16 + (anySub ? 16.0f : 0.0f));
    const float maxH = (float) baseH - 12 - 2 * kMenuPad;
    pn.rows.clear(); float x = 0, y = 0, colH = 0;
    for (auto& e : pn.items) {
        const float h = e.separator ? kMenuSepH : kMenuRowH;
        if (y + h > maxH && y > 0) { x += colW; y = 0; }
        pn.rows.push_back(Rect(x, y, colW, h)); y += h; colH = std::max(colH, y);
    }
    const float W = x + colW + 2 * kMenuPad, H = colH + 2 * kMenuPad;
    float px, py;
    if (asSubmenu) { px = anchor.getRight() - 4; py = anchor.getY() - kMenuPad; if (px + W > (float) baseW - 6) px = anchor.getX() - W + 4; }
    else { px = anchor.getX(); py = anchor.getBottom() + 4; if (py + H > (float) baseH - 6 && anchor.getY() - 4 - H >= 6) py = anchor.getY() - 4 - H; }
    px = juce::jlimit(6.0f, std::max(6.0f, (float) baseW - 6 - W), px); py = juce::jlimit(6.0f, std::max(6.0f, (float) baseH - 6 - H), py);
    pn.r = Rect(px, py, W, H);
    for (auto& rr : pn.rows) rr = rr.translated(px + kMenuPad, py + kMenuPad);
}
void KindredEditor::paintMenu(Graphics& g) {
    static const Path tick = [] { Path p; p.startNewSubPath(0.18f, 0.54f); p.lineTo(0.42f, 0.76f); p.lineTo(0.82f, 0.26f);
        juce::PathStrokeType(0.13f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(p, p); return p; }();
    const auto f = font(1, 12.5f);
    for (size_t k = 0; k < menu->panels.size(); k++) {
        const auto& pn = menu->panels[k];
        juce::DropShadow(juce::Colours::black.withAlpha(th.light ? 0.2f : 0.5f), 20, { 0, 7 }).drawForRectangle(g, pn.r.toNearestInt());
        fillRR(g, pn.r, 9, th.light ? th.card : th.cardHi); strokeRR(g, pn.r, 9, th.light ? th.line : th.faint.withAlpha(0.7f));
        const int openSub = k + 1 < menu->panels.size() ? pn.openRow : -1;
        for (size_t i = 0; i < pn.items.size(); i++) {
            const auto& e = pn.items[i]; const Rect r = pn.rows[i];
            if (e.separator) { g.setColour(th.line.withAlpha(th.light ? 1.0f : 0.9f)); g.fillRect(Rect(r.getX() + 8, r.getCentreY() - 0.5f, r.getWidth() - 16, 1)); continue; }
            const bool hot = e.enabled && ((int) i == pn.hover || (int) i == openSub);
            if (hot) fillRR(g, r.reduced(1, 0.5f), 6, th.accent.withAlpha(th.light ? 0.22f : 0.18f));
            if (e.ticked) icon(g, tick, Rect(r.getX() + 8, r.getCentreY() - 6, 12, 12), e.enabled ? th.accent : th.faint);
            text(g, e.text, r.getX() + kMenuGutter, r.getCentreY() + 4.4f, f, !e.enabled ? th.faint : th.text);
            if (!e.sub.empty()) { g.saveState(); g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, r.getRight() - 12, r.getCentreY()));
                icon(g, chevronPath(), Rect(r.getRight() - 18, r.getCentreY() - 6, 12, 12), e.enabled ? th.dim : th.faint); g.restoreState(); }
        }
    }
}
bool KindredEditor::menuHit(juce::Point<float> p, int& panel, int& row) const {
    panel = row = -1; if (!menu) return false;
    for (int k = (int) menu->panels.size() - 1; k >= 0; k--) {
        const auto& pn = menu->panels[(size_t) k];
        if (!pn.r.contains(p)) continue;
        panel = k;
        for (size_t i = 0; i < pn.rows.size(); i++) if (pn.rows[i].contains(p)) { row = (int) i; break; }
        return true;
    }
    return false;
}
void KindredEditor::menuOpenSub(int panel, int row) {
    auto& pn = menu->panels[(size_t) panel]; const auto& e = pn.items[(size_t) row];
    MenuPanel sub; sub.items = e.sub; layoutMenuPanel(sub, pn.rows[(size_t) row].withX(pn.r.getX()).withWidth(pn.r.getWidth()), true);
    menu->panels.resize((size_t) panel + 1); menu->panels[(size_t) panel].openRow = row; menu->panels.push_back(std::move(sub));
}
void KindredEditor::menuHover(juce::Point<float> p) {
    int panel, row; const bool in = menuHit(p, panel, row);
    if (!in) { auto& last = menu->panels.back(); if (last.hover != -1) { last.hover = -1; repaint(); } return; }
    auto& pn = menu->panels[(size_t) panel];
    const bool usable = row >= 0 && !pn.items[(size_t) row].separator && pn.items[(size_t) row].enabled;
    const int h = usable ? row : -1;
    const bool hasSub = usable && !pn.items[(size_t) row].sub.empty();
    const bool subShown = (int) menu->panels.size() > panel + 1;
    if (h == pn.hover && (hasSub == subShown || h < 0)) return;
    pn.hover = h;
    if (h >= 0) { if (hasSub) menuOpenSub(panel, row); else menu->panels.resize((size_t) panel + 1); }
    repaint();
}
void KindredEditor::menuChoose(int panel, int row) {
    const auto e = menu->panels[(size_t) panel].items[(size_t) row];
    if (e.separator || !e.enabled) return;
    if (!e.sub.empty()) { menuOpenSub(panel, row); repaint(); return; }
    auto fn = std::move(menu->fn); menu.reset(); menusChosen++; updateHover(); repaint();
    if (fn && e.id > 0) fn(e.id);              // the menu is gone before the action runs, so the action may open another
}
bool KindredEditor::menuKey(const juce::KeyPress& k) {
    auto& pn = menu->panels.back(); const int n = (int) pn.items.size();
    auto step = [&](int d) { int i = pn.hover; for (int t = 0; t < n; t++) { i = (i + d + n) % n; if (i < 0) i = d > 0 ? 0 : n - 1; if (!pn.items[(size_t) i].separator && pn.items[(size_t) i].enabled) { pn.hover = i; break; } } };
    if (k == juce::KeyPress::escapeKey) { popupsDismissed++; closePopups(); }
    else if (k == juce::KeyPress::downKey) { if (pn.hover < 0) pn.hover = n - 1; step(1); }
    else if (k == juce::KeyPress::upKey) { if (pn.hover < 0) pn.hover = 0; step(-1); }
    else if (k == juce::KeyPress::leftKey) { if (menu->panels.size() > 1) menu->panels.pop_back(); }
    else if ((k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey || k == juce::KeyPress::rightKey) && pn.hover >= 0) {
        const int panel = (int) menu->panels.size() - 1, row = pn.hover; const bool sub = !pn.items[(size_t) row].sub.empty();
        if (sub) { menuOpenSub(panel, row); menu->panels.back().hover = -1; auto& np = menu->panels.back(); for (size_t i = 0; i < np.items.size(); i++) if (!np.items[i].separator && np.items[i].enabled) { np.hover = (int) i; break; } }
        else if (k != juce::KeyPress::rightKey) menuChoose(panel, row);
    }
    repaint(); return true;                    // while a menu is open, no key reaches the controls under it
}
void KindredEditor::closePopups() {
    if (!menu && !perf) return;
    menu.reset(); perf.reset(); updateHover(); repaint();
}
// A press that the editor never receives: in another window of the host, on the desktop, in
// another application. The timer looks for it while a menu or the panel is open.
void KindredEditor::outsidePress() { if (menu || perf) { popupsDismissed++; closePopups(); } }
double KindredEditor::secondsSinceLastPressAnywhere() {
   #if JUCE_MAC
    double best = 1e30;
    for (uint32_t type : { 1u, 3u, 25u }) best = std::min(best, CGEventSourceSecondsSinceLastEventType(0, type));   // left, right, other button down; 0 = the whole login session
    return best;
   #else
    return -1;
   #endif
}
// Was a mouse press somewhere that this plug-in's own windows did not get? All times in seconds
// on one clock. No screen coordinates and no focus state are used: the host may keep the plug-in
// in another process and show its view in the host's window, and then neither can be trusted.
//   openedAt     when the menu or the panel opened
//   osPressAt    when the system last saw a button go down (negative: the system gives no answer)
//   ownPressAt   when one of our own components last got a mouse-down
//   downSeenAt   when the timer first saw the button down in the current press (negative: it is up)
//   sawRelease   the button was up at some time since openedAt
bool KindredEditor::pressWasElsewhere(double now, double openedAt, double osPressAt, double ownPressAt, double downSeenAt, bool sawRelease) {
    const double settle = 0.09;        // a press of our own reaches us within this time
    // 1. The system's press clock. It also catches a press shorter than one timer period.
    if (osPressAt >= 0 && osPressAt > openedAt + 0.05 && now - osPressAt > settle && ownPressAt < osPressAt - 0.06) return true;
    // 2. The button state: down now, it was up since the menu opened, and no press of our own came with it.
    if (downSeenAt >= 0 && sawRelease && now - downSeenAt > settle && ownPressAt < downSeenAt - 0.12) return true;
    return false;
}
void KindredEditor::watchOutsidePress() {
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    const bool down = juce::ComponentPeer::getCurrentModifiersRealtime().isAnyMouseButtonDown();
    if (!down) { popupSawRelease = true; popupDownSeenAt = -1; } else if (popupDownSeenAt < 0) popupDownSeenAt = now;
    const double since = secondsSinceLastPressAnywhere();
    if (pressWasElsewhere(now, popupOpenedAt, since >= 0 ? now - since : -1.0, ownPressAt, popupDownSeenAt, popupSawRelease)) {
        closedByOutsideAt = now; outsidePress();
    }
}
// Every mouse-down that JUCE delivers in this process comes through here (the editor is a global
// mouse listener). A press on another JUCE window of this process is a press outside the editor.
void KindredEditor::PressTap::mouseDown(const juce::MouseEvent& e) {
    owner.ownPressAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    if ((owner.menu || owner.perf) && e.eventComponent != &owner && !owner.isParentOf(e.eventComponent)) owner.outsidePress();
}
int KindredEditor::menuRowCount(int panel) const { return menu && panel < (int) menu->panels.size() ? (int) menu->panels[(size_t) panel].items.size() : 0; }
Rect KindredEditor::menuBoundsForTest(int panel) const { return menu && panel < (int) menu->panels.size() ? menu->panels[(size_t) panel].r : Rect(); }
Rect KindredEditor::menuRowForTest(int panel, int row, int* id, bool* enabled, bool* hasSub) const {
    if (!menu || panel >= (int) menu->panels.size() || row >= (int) menu->panels[(size_t) panel].items.size()) return {};
    const auto& e = menu->panels[(size_t) panel].items[(size_t) row];
    if (id) *id = e.separator ? 0 : e.id; if (enabled) *enabled = e.enabled && !e.separator; if (hasSub) *hasSub = !e.sub.empty();
    return menu->panels[(size_t) panel].rows[(size_t) row];
}
Rect KindredEditor::perfBoundsForTest() const { return perf ? perf->getBounds().toFloat() : Rect(); }
std::vector<KindredEditor::TestZone> KindredEditor::popupZonesForTest() const {
    std::vector<TestZone> out; bool tab = false, card = false, block = false;
    for (auto& z : zones) switch (z.kind) {
        case StylePill: out.push_back({ "Style", z.r }); break;
        case KeyPill: out.push_back({ "Key", z.r }); break;
        case ModePill: out.push_back({ "Mode", z.r }); break;
        case MeterPill: out.push_back({ "Meter", z.r }); break;
        case TempoPill: if (proc.isStandalone() || !proc.ui.hostTempoKnown.load()) out.push_back({ "Tempo", z.r }); break;
        case LinkPill: out.push_back({ "Link", z.r }); break;
        case MenuBtn: out.push_back({ "Main menu", z.r }); break;
        case TabAdd: out.push_back({ "Add section", z.r }); break;
        case SecRole: out.push_back({ "Section role", z.r }); break;
        case SecLen: out.push_back({ "Section chords", z.r }); break;
        case SecBars: out.push_back({ "Section bars each", z.r }); break;
        case SecPlan: out.push_back({ "Section bass plan", z.r }); break;
        case Tab: if (!tab) { tab = true; out.push_back({ "Tab (right-click)", z.r, true }); } break;
        case Card: if (!card) { card = true; out.push_back({ "Chord card (right-click)", z.r, true }); } break;
        case BlockZ: if (!block) { block = true; out.push_back({ "Arrangement block (right-click)", z.r, true }); } break;
        default: break;
    }
    return out;
}
void KindredEditor::rollSectionIfFree(const std::string& id) {
    const auto p = proc.session().project(); const Section* s = p->section(id); if (!s) return;
    bool locked = false; for (auto& sl : s->slots) if (sl.locked) locked = true;
    if (locked) say("Roll the section to apply the change (locked chords stay).");
    else proc.session().rollSection(id);
}

void KindredEditor::sectionSetting(Kind k, Rect r) {
    const auto p = proc.session().project(); const Section* sec = selected(*p); if (!sec) return;
    const std::string id = sec->id; const Style& st = *proc.session().currentStyle(); juce::PopupMenu m;
    if (k == SecRole) {
        for (size_t i = 0; i < st.roles.size(); i++) m.addItem((int) i + 1, u8(st.roles[i].name), true, st.roles[i].id == sec->role);
        showMenu(m, r, [this, id](int c) { const Style& s2 = *proc.session().currentStyle(); const auto role = s2.roles[(size_t) c - 1].id;
            proc.session().edit([&](Project& q, const Engine&) { if (auto* s = q.section(id)) { s->role = role; s->planPref = "auto"; } }, true, "Role"); rollSectionIfFree(id); });
    } else if (k == SecLen) {
        m.addItem(4, "4 chords", true, sec->length == 4); m.addItem(8, "8 chords", true, sec->length != 4);
        showMenu(m, r, [this, id](int c) { proc.session().edit([&](Project& q, const Engine&) { if (auto* s = q.section(id)) s->length = c; }, true, "Chords"); rollSectionIfFree(id); });
    } else if (k == SecBars) {
        for (int b : { 1, 2, 4 }) m.addItem(b, String(b) + (b == 1 ? " bar each" : " bars each"), true, sec->chordBars == b);
        showMenu(m, r, [this, id](int c) { proc.session().edit([&](Project& q, const Engine&) { if (auto* s = q.section(id)) s->chordBars = c; }, true, "Bars each"); rollSectionIfFree(id); });
    } else if (k == SecPlan) {
        const auto plans = proc.session().engine().allowedPlans(*p, sec->role);
        m.addItem(1, "Auto (the roll chooses)", true, sec->planPref == "auto"); m.addSeparator();
        for (size_t i = 0; i < plans.size(); i++) if (auto* pl = st.plan(plans[i])) m.addItem((int) i + 2, u8(pl->label), true, sec->planPref == plans[i]);
        showMenu(m, r, [this, id, plans](int c) { const std::string pref = c == 1 ? "auto" : plans[(size_t) c - 2];
            proc.session().edit([&](Project& q, const Engine&) { if (auto* s = q.section(id)) s->planPref = pref; }, true, "Bass plan"); rollSectionIfFree(id); });
    }
}
// Rename in place: a text field over the tab (a child of the editor, not an alert window).
// Return keeps the name, Escape drops it, a click anywhere else keeps it.
void KindredEditor::renameSection(const std::string& id) {
    closePopups(); finishRename(true);
    const auto p = proc.session().project(); const Section* sec = p->section(id); if (!sec) return;
    Rect r(250, 66, 150, 30);
    for (auto& z : zones) if (z.kind == Tab && z.a < (int) p->sections.size() && p->sections[(size_t) z.a].id == id) r = z.r;
    nameEdit = std::make_unique<juce::TextEditor>(); nameEditId = id; nameEditDone = false;
    auto& t = *nameEdit;
    t.setFont(realFont(font(1, 13))); t.setJustification(juce::Justification::centredLeft); t.setIndents(8, 0); t.setSelectAllWhenFocused(true);
    t.setColour(juce::TextEditor::backgroundColourId, th.light ? th.card : th.cardHi); t.setColour(juce::TextEditor::textColourId, th.text);
    t.setColour(juce::TextEditor::outlineColourId, th.accent); t.setColour(juce::TextEditor::focusedOutlineColourId, th.accent);
    t.setColour(juce::TextEditor::highlightColourId, th.accent.withAlpha(0.35f)); t.setColour(juce::TextEditor::highlightedTextColourId, th.text);
    t.setColour(juce::CaretComponent::caretColourId, th.text);
    t.setText(u8(sec->name), false); t.setInputRestrictions(40);
    t.setTransform(juce::AffineTransform::scale(scale));
    t.setBounds(r.withWidth(std::max(150.0f, r.getWidth())).reduced(0, 2).toNearestInt());
    t.onReturnKey = [this] { finishRename(true); }; t.onEscapeKey = [this] { finishRename(false); }; t.onFocusLost = [this] { finishRename(true); };
    addAndMakeVisible(t); t.grabKeyboardFocus(); t.selectAll();
}
// Called from the text field's own callbacks, so the field is only hidden here; the timer deletes it.
void KindredEditor::finishRename(bool keep) {
    if (!nameEdit || nameEditDone) return;
    nameEditDone = true; nameEdit->setVisible(false);
    const auto name = nameEdit->getText().trim(); const std::string id = nameEditId;
    if (keep && name.isNotEmpty()) { const auto p = proc.session().project(); const Section* sec = p->section(id);
        if (sec && u8(sec->name) != name) proc.session().edit([&](Project& q, const Engine&) { if (auto* s2 = q.section(id)) s2->name = name.toStdString(); }, true, "Rename"); }
    repaint();
}
void KindredEditor::tabMenu(int index, Rect r) {
    const auto p = proc.session().project(); if (index < 0 || index >= (int) p->sections.size()) return;
    const std::string id = p->sections[(size_t) index].id;
    juce::PopupMenu m; m.addItem(1, "Rename..."); m.addItem(2, "Duplicate"); m.addItem(3, "Add to the arrangement"); m.addSeparator(); m.addItem(4, "Delete section", p->sections.size() > 1);
    showMenu(m, r, [this, id](int c) {
        auto& s = proc.session();
        if (c == 1) renameSection(id);
        else if (c == 2) s.edit([&](Project& q, const Engine&) { if (auto* src = q.section(id)) { Section d = *src; d.id = s.newId(); d.name += " 2"; q.sections.push_back(d); } }, true, "Duplicate section");
        else if (c == 3) s.edit([&](Project& q, const Engine&) { if (auto* src = q.section(id)) q.arrangement.push_back({ s.newId(), id, (int) std::max(1.0, Engine::sectionBars(q, *src)) }); }, true, "Add block");
        else if (c == 4) s.edit([&](Project& q, const Engine&) {
            q.sections.erase(std::remove_if(q.sections.begin(), q.sections.end(), [&](const Section& x) { return x.id == id; }), q.sections.end());
            q.arrangement.erase(std::remove_if(q.arrangement.begin(), q.arrangement.end(), [&](const Block& b) { return b.sectionId == id; }), q.arrangement.end()); }, true, "Delete section");
    });
}
void KindredEditor::cardMenu(int slot, Rect r) {
    const auto p = proc.session().project(); const Section* sec = selected(*p); if (!sec || slot < 0 || slot >= (int) sec->slots.size()) return;
    const std::string id = sec->id; const bool locked = sec->slots[(size_t) slot].locked;
    juce::PopupMenu m; m.addItem(1, locked ? "Unlock" : "Lock"); m.addItem(2, "Re-roll this chord", !locked); m.addSeparator(); m.addItem(3, "Lock all in this section"); m.addItem(4, "Unlock all in this section");
    showMenu(m, r, [this, id, slot](int c) {
        auto& s = proc.session();
        if (c == 2) { s.rerollSlot(id, slot); return; }
        s.edit([&](Project& q, const Engine&) { if (auto* x = q.section(id)) { if (c == 1 && slot < (int) x->slots.size()) x->slots[(size_t) slot].locked = !x->slots[(size_t) slot].locked;
            else if (c == 3 || c == 4) for (auto& sl : x->slots) sl.locked = c == 3; } }, true, "Lock");
    });
}
void KindredEditor::blockMenu(int index, Rect r) {
    const auto p = proc.session().project(); if (index < 0 || index >= (int) p->arrangement.size()) return;
    juce::PopupMenu m, secs; for (size_t i = 0; i < p->sections.size(); i++) secs.addItem(100 + (int) i, u8(p->sections[i].name), true, p->sections[i].id == p->arrangement[(size_t) index].sectionId);
    m.addSubMenu("Section", secs); juce::PopupMenu bars; for (int b : { 1, 2, 4, 8, 12, 16, 24, 32 }) bars.addItem(200 + b, String(b), true, p->arrangement[(size_t) index].bars == b);
    m.addSubMenu("Bars", bars); m.addItem(1, "Duplicate"); m.addSeparator(); m.addItem(2, "Remove block");
    showMenu(m, r, [this, index](int c) {
        auto& s = proc.session();
        s.edit([&](Project& q, const Engine&) {
            if (index >= (int) q.arrangement.size()) return;
            if (c >= 200) q.arrangement[(size_t) index].bars = c - 200;
            else if (c >= 100) { if (c - 100 < (int) q.sections.size()) q.arrangement[(size_t) index].sectionId = q.sections[(size_t) c - 100].id; }
            else if (c == 1) { Block b = q.arrangement[(size_t) index]; b.id = s.newId(); q.arrangement.insert(q.arrangement.begin() + index + 1, b); }
            else if (c == 2) q.arrangement.erase(q.arrangement.begin() + index);
        }, true, "Arrangement");
    });
}
// The performance panel is a child of the editor (no call-out window), in base coordinates and
// scaled with the rest of the picture. It closes the same way a menu does.
void KindredEditor::openPerformance(Rect r) {
    closePopups();
    auto panel = std::make_unique<PerfPanel>(proc, th);
    panel->onEscape = [this] { popupsDismissed++; closePopups(); };
    const float w = (float) panel->getWidth(), h = (float) panel->getHeight();
    float x = r.getRight() - w, y = r.getBottom() + 6; if (y + h > (float) baseH - 6) y = r.getY() - 6 - h;
    panel->setTransform(juce::AffineTransform::scale(scale));
    panel->setTopLeftPosition(juce::roundToInt(juce::jlimit(6.0f, (float) baseW - 6 - w, x)), juce::roundToInt(juce::jlimit(6.0f, std::max(6.0f, (float) baseH - 6 - h), y)));
    addAndMakeVisible(*panel); perf = std::move(panel);
    popupOpenedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0; popupSawRelease = false; popupDownSeenAt = -1;
    updateHover(); repaint();
}
void KindredEditor::openMainMenu(Rect r) {
    juce::PopupMenu m;
    m.addItem(1, "New project"); m.addItem(2, "Open project..."); m.addItem(3, "Save project as..."); m.addSeparator();
    m.addItem(4, "Export arrangement as MIDI file..."); m.addItem(5, "Export this section as MIDI file..."); m.addItem(6, "Copy chord chart"); m.addSeparator();
    m.addItem(7, "Performance settings..."); m.addItem(8, "Follow playback", true, follow); m.addItem(9, "Stop all notes"); m.addSeparator();
    m.addItem(10, "Light theme", true, th.light); m.addItem(11, "Open the Styles folder"); m.addSeparator();
    m.addItem(12, String(KINDRED_PRODUCT_NAME) + " " + KINDRED_VERSION + " (build " + String(KINDRED_BUILD) + ")  " + String::fromUTF8("\xc2\xb7") + "  " + KINDRED_COMPANY_NAME, false);
    showMenu(m, r, [this, r](int c) {
        auto& s = proc.session(); juce::Component::SafePointer<KindredEditor> safe(this);
        const auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto saveBytes = [this, safe, docs](const String& title, const String& name, const String& pattern, std::function<juce::MemoryBlock()> make) {
            chooser = std::make_unique<juce::FileChooser>(title, docs.getChildFile(name), pattern);
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe, make](const juce::FileChooser& fc) {
                const auto f = fc.getResult(); if (!safe || f == juce::File()) return;
                const auto data = make(); safe->say(f.replaceWithData(data.getData(), data.getSize()) ? "Saved " + f.getFileName() + "." : "Could not write " + f.getFileName() + ".");
            });
        };
        switch (c) {
            case 1: s.newProject(); say("New project. Undo brings the old one back."); break;
            case 2:
                chooser = std::make_unique<juce::FileChooser>("Open a project", docs, "*.json;*.kindred");
                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe](const juce::FileChooser& fc) {
                    const auto f = fc.getResult(); if (!safe || f == juce::File()) return;
                    try { safe->proc.session().importProject(Json::parse(f.loadFileAsString().toStdString())); safe->say("Opened " + f.getFileName() + "."); }
                    catch (const std::exception& e) { safe->say("Could not open " + f.getFileName() + ": " + e.what()); }
                });
                break;
            case 3: saveBytes("Save the project", "Kindred project.kindred", "*.kindred", [this] { const auto t = Engine::toJson(*proc.session().project()).dump(1); return juce::MemoryBlock(t.data(), t.size()); }); break;
            case 4: saveBytes("Export the arrangement", "Arrangement.mid", "*.mid", [this] { const auto b = proc.midiFor({}, proc.partChoice()); return juce::MemoryBlock(b.data(), b.size()); }); break;
            case 5: saveBytes("Export this section", "Section.mid", "*.mid", [this] { const auto b = proc.midiFor(proc.session().play().sectionId, proc.partChoice()); return juce::MemoryBlock(b.data(), b.size()); }); break;
            case 6: juce::SystemClipboard::copyTextToClipboard(u8(s.engine().chartText(*s.project()))); say("Chord chart copied."); break;
            case 7: openPerformance(r); break;
            case 8: follow = !follow; break;
            case 9: proc.panic(); proc.stopPreview(); say("All notes stopped."); break;
            case 10: setLightTheme(!th.light); break;
            case 11: { auto f = KindredProcessor::stylesFolder(); f.createDirectory(); f.revealToUser(); break; }
            default: break;
        }
    });
}

void KindredEditor::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const auto pt = toBase(e.position); hoverPt = pt;
    swallowPress = false;
    finishRename(true);
    // A press that closed a menu from outside and then reaches the editor late (a stalled message
    // thread) must not act on the control under it.
    if (juce::Time::getMillisecondCounterHiRes() / 1000.0 - closedByOutsideAt < 0.4) { swallowPress = true; return; }
    if (menu) {                 // a press on a row arms it (mouseUp chooses); a press anywhere else closes the menu and does nothing more
        int panel, row; menu->openerPress = false;
        if (!menuHit(pt, panel, row)) { popupsDismissed++; closePopups(); swallowPress = true; } else menuHover(pt);
        return;
    }
    if (perf) { popupsDismissed++; closePopups(); swallowPress = true; return; }
    const Zone* z = zoneAt(pt); if (!z) return;
    const Zone zc = *z; auto& s = proc.session(); const auto p = s.project(); const Style& st = *s.currentStyle();
    const bool right = e.mods.isPopupMenu();
    dragKind = None; dragMoved = false;
    switch (zc.kind) {
        case Roll: s.rollAll(); say("Rolled every section. Locked chords stayed."); break;
        case Undo: s.undo(); break;
        case Redo: s.redo(); break;
        case SliderZ: dragKind = SliderZ; dragA = zc.a; mouseDrag(e); break;
        case Toggle:
            s.edit([&](Project& q, const Engine&) { if (zc.a == 0) q.tie = !q.tie; else if (zc.a == 1) q.extended = !q.extended; else {
                q.majorModesOnly = !q.majorModesOnly; } }, true, "Switch");
            break;
        case StylePill: {
            juce::PopupMenu m; const auto all = StyleLibrary::get().all();
            for (size_t i = 0; i < all.size(); i++) m.addItem((int) i + 1, u8(all[i].style->name) + (all[i].builtIn ? String() : String("  (user)")), true, all[i].style->id == st.id);
            showMenu(m, zc.r, [this, all](int c) { proc.session().setStyle(all[(size_t) c - 1].style->id, true); say("Style: " + u8(all[(size_t) c - 1].style->name) + ". Rolled every section; locked chords stayed."); });
            break; }
        case KeyPill: {
            juce::PopupMenu m; for (int k = 0; k < 12; k++) m.addItem(k + 1, u8(Engine::keyName(k)), true, p->key == k);
            showMenu(m, zc.r, [this](int c) { proc.session().edit([c](Project& q, const Engine&) { q.key = c - 1; }, true, "Key"); say("Key set. It applies to what you roll from now on."); });
            break; }
        case ModePill: {
            juce::PopupMenu m;
            for (size_t i = 0; i < st.modes.size(); i++) m.addItem((int) i + 1, u8(st.modes[i].name), !(p->majorModesOnly && st.modes[i].minor), st.modes[i].id == p->mode);
            showMenu(m, zc.r, [this](int c) { const auto id = proc.session().currentStyle()->modes[(size_t) c - 1].id;
                proc.session().edit([id](Project& q, const Engine&) { q.mode = id; }, true, "Mode"); say("Mode set. It applies to what you roll from now on."); });
            break; }
        case MeterPill: {
            juce::PopupMenu m; const bool fm = s.play().followMeter;
            if (!proc.isStandalone()) { m.addItem(100, "Follow the host", true, fm); m.addSeparator(); }
            for (int b = 2; b <= 7; b++) m.addItem(b, String(b) + "/4", true, (!fm || proc.isStandalone()) && p->bpb == b);
            showMenu(m, zc.r, [this](int c) {
                if (c == 100) { proc.session().setPlay([](PlayState& ps) { ps.followMeter = true; }); return; }
                proc.session().setPlay([](PlayState& ps) { ps.followMeter = false; });
                proc.session().edit([c](Project& q, const Engine& en) { en.refitMeter(q, c); }, true, "Meter"); say("Meter changed. The chords stayed; bar lengths were refitted.");
            });
            break; }
        case TempoPill:
            if (proc.isStandalone() || !proc.ui.hostTempoKnown.load()) {
                juce::PopupMenu m; for (int t : { 40, 50, 60, 66, 72, 80, 90, 100, 110, 120, 140 }) m.addItem(t, String(t) + " bpm", true, (int) p->tempo == t);
                showMenu(m, zc.r, [this](int c) { proc.session().edit([c](Project& q, const Engine&) { q.tempo = c; }, false); });
            }
            break;
        case Play: if (!proc.ui.hostPlaying.load()) proc.togglePreview(); break;
        case Part: proc.setPart(zc.a, true); repaint(); break;
        case LinkPill: {
            juce::PopupMenu m; m.addItem(1, "Not linked", true, proc.link().isEmpty()); m.addSeparator();
            for (int i = 0; i < 8; i++) { const String l = String::charToString((juce::juce_wchar) ('A' + i)); m.addItem(2 + i, "Link " + l, !proc.isStandalone(), proc.link() == l); }
            showMenu(m, zc.r, [this](int c) { proc.setLink(c == 1 ? String() : String::charToString((juce::juce_wchar) ('A' + c - 2))); seenRev = 0; say(c == 1 ? "Not linked." : "Linked. Instances with this letter share one progression."); });
            break; }
        case MenuBtn: openMainMenu(zc.r); break;
        case Tab:
            if (zc.a < (int) p->sections.size()) { selectSection(p->sections[(size_t) zc.a].id, true); if (right) tabMenu(zc.a, Rect(pt.x, pt.y, 1, 1)); }
            break;
        case TabAdd: {
            juce::PopupMenu m; for (size_t i = 0; i < st.roles.size(); i++) m.addItem((int) i + 1, u8(st.roles[i].name));
            showMenu(m, zc.r, [this](int c) {
                auto& ss = proc.session(); const auto role = ss.currentStyle()->roles[(size_t) c - 1].id; std::string nid;
                ss.edit([&](Project& q, const Engine& en) { Section n = en.newSection(role, [&] { return ss.newId(); }); nid = n.id; q.sections.push_back(n); en.rollSection(q, q.sections.back(), nullptr, ss.newSeed()); }, true, "Add section");
                selectSection(nid, true);
            });
            break; }
        case SecRole: case SecLen: case SecBars: case SecPlan: sectionSetting(zc.kind, zc.r); break;
        case SecRoll: if (auto* sec = selected(*p)) { s.rollSection(sec->id); say("Rolled " + u8(sec->name) + "."); } break;
        case SecLock: if (auto* sec = selected(*p)) { const std::string id = sec->id; bool all = true; for (auto& sl : sec->slots) if (!sl.locked) all = false;
            s.edit([&](Project& q, const Engine&) { if (auto* x = q.section(id)) for (auto& sl : x->slots) sl.locked = !all; }, true, "Lock all"); } break;
        case Scope: s.setPlay([](PlayState& ps) { ps.sectionScope = !ps.sectionScope; }); break;
        case CardLock: if (auto* sec = selected(*p)) { const std::string id = sec->id; const int i = zc.a;
            s.edit([&](Project& q, const Engine&) { if (auto* x = q.section(id)) if (i < (int) x->slots.size()) x->slots[(size_t) i].locked = !x->slots[(size_t) i].locked; }, true, "Lock"); } break;
        case CardDice: if (auto* sec = selected(*p)) s.rerollSlot(sec->id, zc.a); break;
        case Card:
            if (auto* sec = selected(*p)) {
                selSlot = zc.a;
                if (right) { cardMenu(zc.a, Rect(pt.x, pt.y, 1, 1)); break; }
                if (e.mods.isAltDown()) { const std::string id = sec->id; const int i = zc.a;
                    s.edit([&](Project& q, const Engine&) { if (auto* x = q.section(id)) x->slots[(size_t) i].locked = !x->slots[(size_t) i].locked; }, true, "Lock"); break; }
                if (zc.a < (int) sec->slots.size() && sec->slots[(size_t) zc.a].chord) { const auto& ch = *sec->slots[(size_t) zc.a].chord; proc.audition(ch.notes, ch.bass.value_or(-1)); }
                repaint();
            }
            break;
        case BlockZ:
            if (zc.a < (int) p->arrangement.size()) {
                selectSection(p->arrangement[(size_t) zc.a].sectionId, true);
                if (right) blockMenu(zc.a, Rect(pt.x, pt.y, 1, 1)); else { dragKind = BlockZ; dragA = zc.a; dragStartX = pt.x; dropIndex = -1; }
            }
            break;
        case BlockEdge: if (zc.a < (int) p->arrangement.size()) { dragKind = BlockEdge; dragA = zc.a; dragStartX = pt.x; dragStartBars = p->arrangement[(size_t) zc.a].bars; } break;
        case BlockAdd: if (auto* sec = selected(*p)) { const std::string id = sec->id; const int bars = (int) std::max(1.0, Engine::sectionBars(*p, *sec));
            s.edit([&](Project& q, const Engine&) { q.arrangement.push_back({ s.newId(), id, bars }); }, true, "Add block"); } break;
        case LoopToggle: s.setPlay([](PlayState& ps) { ps.loop = !ps.loop; }); break;
        case DragSection: case DragArr: dragKind = zc.kind; dragStartX = pt.x; break;
        default: break;
    }
}
void KindredEditor::mouseDrag(const juce::MouseEvent& e) {
    const auto pt = toBase(e.position); auto& s = proc.session();
    if (menu) { hoverPt = pt; menuHover(pt); return; }      // press, drag to a row, release: the menu follows the pointer
    if (swallowPress) return;
    if (dragKind == SliderZ) {
        const Style& st = *s.currentStyle(); if (dragA < 0 || dragA >= (int) st.spectra.size()) return;
        const auto& sp = st.spectra[(size_t) dragA];
        double v = sp.min + (sp.max - sp.min) * juce::jlimit(0.0, 1.0, (double) (pt.x - 16) / 200.0); v = std::round(v * 100) / 100;
        const std::string id = sp.id;
        if (const double* cur = s.project()->mood.byId(id)) if (*cur != v) s.edit([&](Project& q, const Engine&) { if (double* m = q.mood.byId(id)) *m = v; }, false);
    } else if (dragKind == BlockEdge) {
        const auto p = s.project(); double total = 0; for (auto& b : p->arrangement) total += b.bars;
        if (dragA >= (int) p->arrangement.size() || total <= 0) return;
        static double pxPerBar = 1; if (!dragMoved) { pxPerBar = (kMainR - 250) / total; dragMoved = true; }
        const int bars = juce::jlimit(1, 128, dragStartBars + (int) std::lround((pt.x - dragStartX) / pxPerBar)); const int i = dragA;
        if (p->arrangement[(size_t) i].bars != bars) s.edit([&](Project& q, const Engine&) { q.arrangement[(size_t) i].bars = bars; }, false);
    } else if (dragKind == BlockZ) {
        if (std::fabs(pt.x - dragStartX) > 6) dragMoved = true;
        if (dragMoved) {
            const auto p = s.project(); double total = 0; for (auto& b : p->arrangement) total += b.bars;
            double bar = 0; dropIndex = (int) p->arrangement.size();
            for (size_t i = 0; i < p->arrangement.size(); i++) { const float mid = 250 + (float) ((bar + p->arrangement[i].bars / 2.0) / total) * (kMainR - 250); if (pt.x < mid) { dropIndex = (int) i; break; } bar += p->arrangement[i].bars; }
            repaint();
        }
    } else if ((dragKind == DragSection || dragKind == DragArr) && std::fabs(pt.x - dragStartX) + std::fabs(e.position.y - e.mouseDownPosition.y) > 5) {
        const auto file = proc.writeDragFile(dragKind == DragArr ? std::string() : s.play().sectionId, proc.partChoice());
        dragKind = None;
        if (file.existsAsFile()) { say("Dragging " + file.getFileName()); performExternalDragDropOfFiles({ file.getFullPathName() }, false, this); }
    }
}
void KindredEditor::mouseUp(const juce::MouseEvent& e) {
    auto& s = proc.session();
    if (menu) {
        // The release of the press that opened the menu chooses a row only after a real drag to it
        // (far enough, or long enough), so a quick click can never open a menu and pick from it at once.
        const auto pt = toBase(e.position); int panel, row;
        const bool opener = menu->openerPress; menu->openerPress = false;
        const bool deliberate = !opener || pt.getDistanceFrom(menu->openerPt) > 6 || juce::Time::getMillisecondCounter() - menu->openedAt > 300;
        if (menuHit(pt, panel, row) && row >= 0 && deliberate) menuChoose(panel, row);
        return;
    }
    if (swallowPress) { swallowPress = false; return; }
    if (dragKind == BlockZ && dragMoved && dropIndex >= 0) {
        const int from = dragA, to = dropIndex;
        if (to != from && to != from + 1) s.edit([&](Project& q, const Engine&) {
            if (from >= (int) q.arrangement.size()) return;
            Block b = q.arrangement[(size_t) from]; q.arrangement.erase(q.arrangement.begin() + from);
            q.arrangement.insert(q.arrangement.begin() + juce::jlimit(0, (int) q.arrangement.size(), to > from ? to - 1 : to), b); }, true, "Move block");
    } else if (dragKind == BlockEdge && dragMoved) s.edit([](Project&, const Engine&) {}, true, "Block length");
    else if (dragKind == DragSection || dragKind == DragArr) say("Drag this tile to a track in your DAW.");
    dragKind = None; dragMoved = false; dropIndex = -1; repaint();
}
void KindredEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if (menu || perf || swallowPress) return;
    const Zone* z = zoneAt(toBase(e.position)); if (!z) return;
    auto& s = proc.session();
    if (z->kind == SliderZ) { const Style& st = *s.currentStyle(); if (z->a < (int) st.spectra.size()) { const auto sp = st.spectra[(size_t) z->a];
        s.edit([&](Project& q, const Engine&) { if (double* m = q.mood.byId(sp.id)) *m = sp.def; }, false); } }
    else if (z->kind == Tab) { const auto p = s.project(); if (z->a < (int) p->sections.size()) renameSection(p->sections[(size_t) z->a].id); }
}
void KindredEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    const Zone* z = menu || perf ? nullptr : zoneAt(toBase(e.position));
    if (z && z->kind == TempoPill && (proc.isStandalone() || !proc.ui.hostTempoKnown.load()) && w.deltaY != 0) {
        const double d = w.deltaY > 0 ? 1 : -1;
        proc.session().edit([d](Project& q, const Engine&) { q.tempo = juce::jlimit(20.0, 300.0, std::round(q.tempo) + d); }, false);
    }
}
bool KindredEditor::keyPressed(const juce::KeyPress& k) {
    auto& s = proc.session();
    if (menu) return menuKey(k);
    if (perf && k == juce::KeyPress::escapeKey) { popupsDismissed++; closePopups(); return true; }
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) { s.undo(); return true; }
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { s.redo(); return true; }
    if (k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown()) return false;
    if (k.getKeyCode() == 'R') { if (k.getModifiers().isShiftDown()) { if (auto* sec = selected(*s.project())) s.rollSection(sec->id); } else s.rollAll(); return true; }
    if (k == juce::KeyPress::spaceKey && proc.isStandalone()) { proc.togglePreview(); return true; }
    if (k.getKeyCode() == 'L' && selSlot >= 0) { if (auto* sec = selected(*s.project())) { const std::string id = sec->id; const int i = selSlot;
        s.edit([&](Project& q, const Engine&) { if (auto* x = q.section(id)) if (i < (int) x->slots.size()) x->slots[(size_t) i].locked = !x->slots[(size_t) i].locked; }, true, "Lock"); } return true; }
    return false;
}

}  // namespace kindred
