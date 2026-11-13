// KindredSnapshot -- builds the processor and the editor in-process and writes PNGs of the editor
// in several states and sizes, with paint timings (developer tool, not shipped).
//   KindredSnapshot <out-dir>
#include <juce_audio_utils/juce_audio_utils.h>
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace juce;
using namespace kindred;

static void pump(int ms) {
    const auto end = Time::getMillisecondCounter() + (uint32) ms;
    while (Time::getMillisecondCounter() < end) MessageManager::getInstance()->runDispatchLoopUntil(10);
}
static void save(Component& c, const File& f, float scale) {
    { auto prime = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f); }      // first paint starts the change animation
    pump(600);                                                                       // let it finish
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, scale);
    f.deleteFile(); FileOutputStream os(f); PNGImageFormat().writeImageToStream(img, os);
    std::printf("wrote %s (%d x %d px, window %d x %d)\n", f.getFileName().toRawUTF8(), img.getWidth(), img.getHeight(), c.getWidth(), c.getHeight());
}
static void timePaint(KindredEditor& ed, const char* what, float scale) {
    double total = 0, worst = 0; const int N = 40;
    for (int i = 0; i < N; i++) { const double t0 = Time::getMillisecondCounterHiRes(); auto img = ed.createComponentSnapshot(ed.getLocalBounds(), true, scale); const double dt = Time::getMillisecondCounterHiRes() - t0; total += dt; worst = jmax(worst, dt); }
    std::printf("paint time, %s, %d x %d at %.0fx: full repaint mean %.2f ms, worst %.2f ms over %d (16.7 ms is one 60 Hz frame)\n", what, ed.getWidth(), ed.getHeight(), scale, total / N, worst, N);
    // What a playing frame repaints: the stage and the arrangement only.
    const float k = (float) ed.getWidth() / 1120.0f; total = 0; worst = 0;
    const Rectangle<int> stage = (Rectangle<float>(240, 104, 872, 552) * k).getSmallestIntegerContainer();
    for (int i = 0; i < N; i++) { const double t0 = Time::getMillisecondCounterHiRes(); auto img = ed.createComponentSnapshot(stage, true, scale); const double dt = Time::getMillisecondCounterHiRes() - t0; total += dt; worst = jmax(worst, dt); }
    std::printf("            playing frame (stage + arrangement, %d x %d px): mean %.2f ms, worst %.2f ms\n", roundToInt(stage.getWidth() * scale), roundToInt(stage.getHeight() * scale), total / N, worst);
}

// Tools never write the user's settings: point Kindred at a scratch settings file first.
static void useScratchSettings() {
    const auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kindred-tools.settings").getFullPathName();
   #ifdef _WIN32
    _putenv_s("KINDRED_SETTINGS_FILE", f.toRawUTF8());
   #else
    setenv("KINDRED_SETTINGS_FILE", f.toRawUTF8(), 1);
   #endif
}
int main(int argc, char** argv) {
    useScratchSettings();
    ScopedJuceInitialiser_GUI init;
    const File dir(argc > 1 ? File::getCurrentWorkingDirectory().getChildFile(argv[1]) : File::getCurrentWorkingDirectory());
    dir.createDirectory();
    auto proc = std::make_unique<KindredProcessor>();
    proc->setRateAndBufferSizeDetails(48000.0, 256); proc->prepareToPlay(48000.0, 256);
    auto& s = proc->session();
    uint32_t seed = argc > 2 ? (uint32_t) String(argv[2]).getLargeIntValue() : 11;
    s.edit([&](Project& p, const Engine& e) { p.key = 0; e.rollAll(p, [&] { return seed++; }); }, false);
    KindredProcessor::settings().setValue("lightTheme", false);

    auto ed = std::unique_ptr<KindredEditor>(dynamic_cast<KindredEditor*>(proc->createEditor()));
    ed->setSize(1120, 720); ed->setVisible(true); pump(150);
    save(*ed, dir.getChildFile("01-default-dark.png"), 2.0f);

    // Lock three chords, then a new roll around them, playing in bar 3 of the verse.
    s.edit([&](Project& p, const Engine&) { auto& sl = p.sections[0].slots; sl[0].locked = sl[1].locked = sl[4].locked = true; }, false);
    s.edit([&](Project& p, const Engine& e) { e.rollAll(p, [&] { return seed++; }); }, false);
    proc->ui.playing.store(true); proc->ui.hostPlaying.store(true); proc->ui.hostTempoKnown.store(true); proc->ui.bpm.store(72.0); proc->ui.pos.store(10.6);
    pump(600);
    save(*ed, dir.getChildFile("02-playing-locked-dark.png"), 2.0f);
    timePaint(*ed, "playing", 2.0f);

    ed->setHoverForTest({ 520.0f, 190.0f }); pump(100);
    save(*ed, dir.getChildFile("03-hover-card-dark.png"), 2.0f);
    ed->setHoverForTest({ -1.0f, -1.0f });

    // The middle eight in the arrangement, section follows playback.
    proc->ui.pos.store(4.0 * 34 + 1.3); pump(300);
    save(*ed, dir.getChildFile("04-middle-eight-dark.png"), 2.0f);

    ed->setLightTheme(true); pump(100);
    save(*ed, dir.getChildFile("05-playing-light.png"), 2.0f);
    proc->ui.playing.store(false); proc->ui.hostPlaying.store(false); pump(100);
    save(*ed, dir.getChildFile("06-stopped-light.png"), 2.0f);
    ed->setLightTheme(false);

    ed->setSize(840, 540); pump(100);
    save(*ed, dir.getChildFile("07-small-840-dark.png"), 2.0f);
    timePaint(*ed, "stopped", 2.0f);
    ed->setSize(1680, 1080); pump(100);
    save(*ed, dir.getChildFile("08-large-1680-dark.png"), 1.0f);
    proc->ui.playing.store(true); proc->ui.pos.store(21.5);
    timePaint(*ed, "playing", 2.0f);
    ed->setSize(2240, 1440); pump(50);
    timePaint(*ed, "playing, largest size", 2.0f);
    proc->ui.playing.store(false);

    // Other content: Lydian over a drone, four chords two bars each, Edge up.
    ed->setSize(1120, 720);
    s.edit([&](Project& p, const Engine& e) { p.key = 5; p.mode = "lydian"; p.mood.moving = 0.15; p.mood.color = 0.8; p.mood.inner = 0.9; p.sections[0].length = 4; p.sections[0].chordBars = 2;
        for (auto& sec : p.sections) for (auto& sl : sec.slots) sl.locked = false; e.rollAll(p, [&] { return seed++; }); }, false);
    s.setPlay([&](PlayState& ps) { ps.sectionId = s.project()->sections[0].id; });
    pump(700);
    save(*ed, dir.getChildFile("09-lydian-four-chords-dark.png"), 2.0f);
    // Menus and the performance panel (drawn inside the editor).
    auto tap = [&](Point<float> p, bool right) {
        const auto mods = right ? ModifierKeys::rightButtonModifier : ModifierKeys::leftButtonModifier;
        MouseEvent e(Desktop::getInstance().getMainMouseSource(), p, mods, 1.0f, 0, 0, 0, 0, ed.get(), ed.get(), Time::getCurrentTime(), p, Time::getCurrentTime(), 1, false);
        ed->mouseMove(e); ed->mouseDown(e); ed->mouseUp(e);
    };
    auto zoneOf = [&](const String& name) { { auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f); } for (auto& z : ed->popupZonesForTest()) if (z.name == name) return z.r; return Rectangle<float>(); };
    auto hoverRow = [&](int panel, int row) { const auto p = ed->menuRowForTest(panel, row).getCentre();
        MouseEvent e(Desktop::getInstance().getMainMouseSource(), p, ModifierKeys(), 0.0f, 0, 0, 0, 0, ed.get(), ed.get(), Time::getCurrentTime(), p, Time::getCurrentTime(), 0, false); ed->mouseMove(e); };
    tap(zoneOf("Key").getCentre(), false); hoverRow(0, 7);
    save(*ed, dir.getChildFile("10-menu-key-dark.png"), 2.0f);
    timePaint(*ed, "menu open", 2.0f);
    tap(zoneOf("Key").getCentre(), false);
    tap(zoneOf("Arrangement block (right-click)").getCentre(), true); hoverRow(0, 1);
    save(*ed, dir.getChildFile("11-menu-block-submenu-dark.png"), 2.0f);
    ed->keyPressed(KeyPress(KeyPress::escapeKey));
    tap(zoneOf("Main menu").getCentre(), false);
    save(*ed, dir.getChildFile("12-menu-main-dark.png"), 2.0f);
    for (int r = 0; r < ed->menuRowCount(0); r++) { int id = 0; const auto rr = ed->menuRowForTest(0, r, &id); if (id == 7) { tap(rr.getCentre(), false); break; } }
    save(*ed, dir.getChildFile("13-performance-panel-dark.png"), 2.0f);
    ed->keyPressed(KeyPress(KeyPress::escapeKey));
    ed->setLightTheme(true); tap(zoneOf("Mode").getCentre(), false); hoverRow(0, 1);
    save(*ed, dir.getChildFile("14-menu-mode-light.png"), 2.0f);
    ed->keyPressed(KeyPress(KeyPress::escapeKey));
    ed->setSize(840, 540); ed->setLightTheme(false); pump(50);
    tap(zoneOf("Main menu").getCentre(), false);
    for (int r = 0; r < ed->menuRowCount(0); r++) { int id = 0; const auto rr = ed->menuRowForTest(0, r, &id); if (id == 7) { tap(rr.getCentre(), false); break; } }
    save(*ed, dir.getChildFile("15-performance-panel-small-840-dark.png"), 2.0f);
    ed->keyPressed(KeyPress(KeyPress::escapeKey));
    KindredProcessor::settings().setValue("lightTheme", false);
    ed.reset(); proc.reset();
    return 0;
}
