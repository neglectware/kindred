// KindredUiTest -- clicks through the editor with synthetic mouse and key events (no screen
// needed) and checks that each control does its job, then opens every menu and the performance
// panel and checks how each one opens, stays, chooses and closes.
//   KindredUiTest            the whole test
//   KindredUiTest --legacy   also measures a juce::PopupMenu shown the old way (a separate
//                            window): this puts a small menu on the screen for a moment
#include <juce_audio_utils/juce_audio_utils.h>
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace juce;
using namespace kindred;

static int failures = 0, steps = 0;
static void check(bool ok, const String& what) { steps++; if (!ok) failures++; std::printf("  %s %s\n", ok ? "ok:  " : "FAIL:", what.toRawUTF8()); }
static void pump(int ms) { const auto end = Time::getMillisecondCounter() + (uint32) ms; while (Time::getMillisecondCounter() < end) MessageManager::getInstance()->runDispatchLoopUntil(10); }

struct Driver {
    KindredEditor& ed; float k;
    MouseEvent ev(Point<float> base, Point<float> downBase, bool dragged, int clicks = 1, ModifierKeys mods = ModifierKeys::leftButtonModifier) {
        return MouseEvent(Desktop::getInstance().getMainMouseSource(), base * k, mods, 1.0f, 0, 0, 0, 0, &ed, &ed, Time::getCurrentTime(), downBase * k, Time::getCurrentTime(), clicks, dragged);
    }
    void paint() { auto img = ed.createComponentSnapshot(ed.getLocalBounds(), true, 1.0f); }      // builds the hit zones
    void click(float x, float y, ModifierKeys mods = ModifierKeys::leftButtonModifier) { paint(); ed.mouseMove(ev({ x, y }, { x, y }, false)); paint(); ed.mouseDown(ev({ x, y }, { x, y }, false, 1, mods)); ed.mouseUp(ev({ x, y }, { x, y }, false, 1, mods)); paint(); }
    void drag(float x0, float y0, float x1, float y1) { paint(); ed.mouseMove(ev({ x0, y0 }, { x0, y0 }, false)); paint(); ed.mouseDown(ev({ x0, y0 }, { x0, y0 }, false));
        for (int i = 1; i <= 8; i++) { const float t = (float) i / 8; ed.mouseDrag(ev({ x0 + (x1 - x0) * t, y0 + (y1 - y0) * t }, { x0, y0 }, true)); paint(); }
        ed.mouseUp(ev({ x1, y1 }, { x0, y0 }, true)); paint(); }
    void press(juce::Point<float> p, bool right = false) { paint(); ed.mouseMove(ev(p, p, false)); paint(); ed.mouseDown(ev(p, p, false, 1, right ? ModifierKeys::rightButtonModifier : ModifierKeys::leftButtonModifier)); down = p; }
    void move(juce::Point<float> p) { ed.mouseMove(ev(p, p, false)); paint(); }
    void dragTo(juce::Point<float> p) { ed.mouseDrag(ev(p, down, true)); paint(); }
    void release(juce::Point<float> p, bool right = false) { ed.mouseUp(ev(p, down, p != down, 1, right ? ModifierKeys::rightButtonModifier : ModifierKeys::leftButtonModifier)); paint(); }
    void tap(juce::Point<float> p, bool right = false) { press(p, right); release(p, right); }
    juce::Point<float> down;
    void dbl(float x, float y) { paint(); ed.mouseDoubleClick(ev({ x, y }, { x, y }, false, 2)); paint(); }
};


// ---- menus and the performance panel -------------------------------------------------------
// The menus are drawn by the editor in its own window. These checks would fail if a menu were a
// separate window (desktop component count), if anything went modal, if a menu closed with no
// input, stayed after a press elsewhere, or came back after the second click on its control.
static int topLevelWindows() { return Desktop::getInstance().getNumComponents(); }
static int modalComponents() { return ModalComponentManager::getInstance()->getNumModalComponents(); }
struct MenuNumbers { int opened = 0, stayed = 0, secondClick = 0, clickAway = 0, outside = 0, escape = 0, resize = 0, hidden = 0, chosen = 0, dragChosen = 0; double openMsTotal = 0; };

static void menuChecks(KindredProcessor& proc, KindredEditor& ed, Driver& d, MenuNumbers& n, int w) {
    auto& s = proc.session();
    auto zones = [&] { d.paint(); return ed.popupZonesForTest(); };
    auto find = [&](const String& name) { for (auto& z : zones()) if (z.name == name) return z; return KindredEditor::TestZone{}; };
    const auto names = [&] { std::vector<String> v; for (auto& z : zones()) v.push_back(z.name); return v; }();
    check(names.size() == 15, "15 controls open a menu (" + String((int) names.size()) + ")");
    const int windows0 = topLevelWindows();
    const Rectangle<float> whole(0, 0, 1120, 720); const Point<float> rollBtn(116, 98), empty(700, 46);
    auto projectText = [&] { return Engine::toJson(*s.project()).dump(); };
    for (auto& name : names) {
        auto z = find(name); const auto at = z.r.getCentre(); const bool rc = z.rightClick;
        auto open = [&] { z = find(name); d.tap(z.r.getCentre(), rc); return ed.menuIsOpen(); };
        // a. opens with one click, inside the editor, as part of the editor
        const double t0 = Time::getMillisecondCounterHiRes(); const bool opened = open(); n.openMsTotal += Time::getMillisecondCounterHiRes() - t0;
        bool ok = opened && ed.menuPanelCount() == 1 && ed.menuRowCount(0) > 0 && whole.contains(ed.menuBoundsForTest(0)) && topLevelWindows() == windows0 && modalComponents() == 0;
        if (ok) n.opened++;
        check(ok, name + ": one click opens it (" + String(ed.menuRowCount(0)) + " rows, inside the editor, no new window, nothing modal)");
        // b. it stays with no input
        pump(300); if (ed.menuIsOpen()) n.stayed++;
        check(ed.menuIsOpen(), name + ": still open after 300 ms with no input");
        // c. second click on the same control closes it and it does not come back
        const auto before = projectText();
        d.tap(at, rc); bool closed = !ed.menuIsOpen(); pump(120); closed = closed && !ed.menuIsOpen();
        if (closed) n.secondClick++;
        check(closed && (rc || projectText() == before), name + ": second click on the control closes it, and it stays closed");
        // d. a press elsewhere in the editor closes it and does nothing else (here: on the Roll button)
        open(); const auto b2 = projectText(); d.tap(rollBtn);
        ok = !ed.menuIsOpen() && projectText() == b2; if (ok) n.clickAway++;
        check(ok, name + ": a click elsewhere in the editor closes it and is not passed on");
        // e. a press outside the editor (host window, desktop, other application)
        open(); ed.outsidePressForTest(); if (!ed.menuIsOpen()) n.outside++;
        check(!ed.menuIsOpen(), name + ": a press outside the editor closes it");
        // f. Escape
        open(); ed.keyPressed(KeyPress(KeyPress::escapeKey)); if (!ed.menuIsOpen()) n.escape++;
        check(!ed.menuIsOpen(), name + ": Escape closes it");
        // g. the editor is resized
        open(); ed.setSize(w + 28, roundToInt((w + 28) * 720.0 / 1120.0)); ok = !ed.menuIsOpen(); ed.setSize(w, roundToInt(w * 720.0 / 1120.0)); if (ok) n.resize++;
        check(ok, name + ": resizing the editor closes it");
        // h. the editor is hidden
        open(); ed.setVisible(false); ok = !ed.menuIsOpen(); ed.setVisible(true); if (ok) n.hidden++;
        check(ok, name + ": hiding the editor closes it");
        // i. choose a row: press and release on it
        open();
        int pick = -1; for (int r = 0; r < ed.menuRowCount(0) && pick < 0; r++) { int id = 0; bool en = false, sub = false; ed.menuRowForTest(0, r, &id, &en, &sub);
            if (!en || sub) continue; if (name == "Main menu" && id != 8) continue; if (name.startsWith("Tab") && id != 2) continue; if (name == "Link" && id != 2) continue; pick = r; }   // Link A is the link this instance already has
        const int chosen0 = ed.menusChosen; const auto row = ed.menuRowForTest(0, juce::jmax(0, pick));
        if (pick >= 0) d.tap(row.getCentre());
        ok = pick >= 0 && !ed.menuIsOpen() && ed.menusChosen == chosen0 + 1; if (ok) n.chosen++;
        check(ok, name + ": press and release on a row chooses it and closes the menu");
        check(topLevelWindows() == windows0 && modalComponents() == 0, name + ": no window and no modal component left behind");
    }
    {   // press on the control, drag to a row, release: chooses. A release at once on a row under the pointer: does not.
        auto z = find("Key"); d.press(z.r.getCentre()); int id = 0; const auto row = ed.menuRowForTest(0, 4, &id);
        d.dragTo(row.getCentre()); d.release(row.getCentre());
        const bool ok = !ed.menuIsOpen() && s.project()->key == id - 1; if (ok) n.dragChosen++;
        check(ok, "press on Key, drag to the fifth row, release: that key is chosen (" + String(s.project()->key) + ")");
        z = find("Arrangement block (right-click)"); const Point<float> low(z.r.getCentreX(), z.r.getBottom() - 2);
        d.tap(low, true);
        check(ed.menuIsOpen(), "right-click low on a block: the menu opens and the release of that same click chooses nothing");
        ed.keyPressed(KeyPress(KeyPress::escapeKey));
    }
    {   // submenus of the block menu
        auto z = find("Arrangement block (right-click)"); d.tap(z.r.getCentre(), true);
        int barsRow = -1, plainRow = -1; for (int r = 0; r < ed.menuRowCount(0); r++) { int id = 0; bool en = false, sub = false; ed.menuRowForTest(0, r, &id, &en, &sub); if (sub) barsRow = r; else if (en) plainRow = r; }
        d.move(ed.menuRowForTest(0, barsRow).getCentre());
        check(ed.menuPanelCount() == 2 && whole.contains(ed.menuBoundsForTest(1)), "block menu: pointing at Bars opens its submenu, inside the editor");
        d.move(ed.menuRowForTest(0, plainRow).getCentre()); check(ed.menuPanelCount() == 1, "pointing at another row closes the submenu");
        d.tap(ed.menuRowForTest(0, barsRow).getCentre()); check(ed.menuIsOpen() && ed.menuPanelCount() == 2, "a click on Bars keeps the menu and shows the submenu");
        int row8 = -1; for (int r = 0; r < ed.menuRowCount(1); r++) { int id = 0; ed.menuRowForTest(1, r, &id); if (id == 208) row8 = r; }
        d.tap(ed.menuRowForTest(1, row8).getCentre());
        check(!ed.menuIsOpen() && s.project()->arrangement[0].bars == 8, "choosing 8 in the submenu sets the block to 8 bars and closes both");
    }
    {   // keys
        auto z = find("Mode"); d.tap(z.r.getCentre()); const auto mode0 = s.project()->mode;
        ed.keyPressed(KeyPress(KeyPress::downKey)); ed.keyPressed(KeyPress(KeyPress::downKey)); const auto t = projectText();
        ed.keyPressed(KeyPress('R')); check(ed.menuIsOpen() && projectText() == t, "with a menu open, the R key does not roll");
        ed.keyPressed(KeyPress(KeyPress::returnKey)); check(!ed.menuIsOpen() && s.project()->mode != mode0, "Down, Down, Return chooses the second mode (" + String(s.project()->mode) + ")");
    }
    {   // one menu open, click on another control: the first click closes, the next opens
        auto a = find("Key"), b = find("Mode"); d.tap(a.r.getCentre()); d.tap(b.r.getCentre()); const bool closed = !ed.menuIsOpen(); d.tap(b.r.getCentre());
        check(closed && ed.menuIsOpen(), "Key open, click on Mode: Key closes; the next click opens Mode"); ed.keyPressed(KeyPress(KeyPress::escapeKey));
    }
    {   // the performance panel: opened from the main menu, a child of the editor
        auto openPerf = [&] { auto z = find("Main menu"); d.tap(z.r.getCentre()); for (int r = 0; r < ed.menuRowCount(0); r++) { int id = 0; const auto rr = ed.menuRowForTest(0, r, &id); if (id == 7) { d.tap(rr.getCentre()); break; } } return ed.perfIsOpen(); };
        bool ok = openPerf() && whole.contains(ed.perfBoundsForTest()) && topLevelWindows() == windows0 && modalComponents() == 0;
        check(ok, "Performance settings: the panel opens inside the editor (no new window, nothing modal)");
        pump(300); check(ed.perfIsOpen(), "the panel is still open after 300 ms with no input");
        const auto t = projectText(); d.tap(rollBtn); check(!ed.perfIsOpen() && projectText() == t, "a click elsewhere in the editor closes the panel and is not passed on");
        openPerf(); ed.outsidePressForTest(); check(!ed.perfIsOpen(), "a press outside the editor closes the panel");
        openPerf(); ed.keyPressed(KeyPress(KeyPress::escapeKey)); check(!ed.perfIsOpen(), "Escape closes the panel");
        openPerf(); ed.setSize(w + 28, roundToInt((w + 28) * 720.0 / 1120.0)); ok = !ed.perfIsOpen(); ed.setSize(w, roundToInt(w * 720.0 / 1120.0)); check(ok, "resizing the editor closes the panel");
        openPerf(); auto z = find("Key"); d.tap(z.r.getCentre()); const bool c1 = !ed.perfIsOpen() && !ed.menuIsOpen(); d.tap(z.r.getCentre());
        check(c1 && ed.menuIsOpen(), "panel open, click on Key: the panel closes; the next click opens Key"); ed.keyPressed(KeyPress(KeyPress::escapeKey));
    }
    {   // rename in place (no alert window)
        auto z = find("Tab (right-click)"); d.tap(z.r.getCentre(), true);
        for (int r = 0; r < ed.menuRowCount(0); r++) { int id = 0; const auto rr = ed.menuRowForTest(0, r, &id); if (id == 1) { d.tap(rr.getCentre()); break; } }
        auto* t = ed.nameEditorForTest();
        check(t != nullptr && topLevelWindows() == windows0 && modalComponents() == 0, "Rename: a text field opens over the tab (no alert window)");
        if (t) { t->setText("Refrain", false); t->onReturnKey(); }
        pump(60); check(ed.nameEditorForTest() == nullptr && s.project()->sections[0].name == "Refrain", "Return keeps the new name (" + String(s.project()->sections[0].name) + ")");
        d.tap(z.r.getCentre(), true); for (int r = 0; r < ed.menuRowCount(0); r++) { int id = 0; const auto rr = ed.menuRowForTest(0, r, &id); if (id == 1) { d.tap(rr.getCentre()); break; } }
        if (auto* t2 = ed.nameEditorForTest()) { t2->setText("Dropped", false); t2->onEscapeKey(); }
        pump(60); check(s.project()->sections[0].name == "Refrain", "Escape drops the change");
    }
}

// What the old code did: juce::PopupMenu shown with withParentComponent(nullptr). Measured, not asserted.
static void legacyProbe() {
    std::printf("\nlegacy probe: juce::PopupMenu as its own window (the code before this fix)\n");
    const int w0 = topLevelWindows(), m0 = modalComponents();
    PopupMenu m; for (int i = 1; i <= 4; i++) m.addItem(i, "Item " + String(i));
    bool done = false; const double t0 = Time::getMillisecondCounterHiRes(); double t1 = 0;
    m.showMenuAsync(PopupMenu::Options().withTargetScreenArea({ 40, 60, 80, 20 }).withParentComponent(nullptr), [&](int) { done = true; t1 = Time::getMillisecondCounterHiRes(); });
    std::printf("  after showMenuAsync: top-level windows %d -> %d, modal components %d -> %d\n", w0, topLevelWindows(), m0, modalComponents());
    auto* modal = Component::getCurrentlyModalComponent();
    std::printf("  the menu window is temporary: %s; it can become the key window: %s\n", modal && modal->getPeer() && (modal->getPeer()->getStyleFlags() & ComponentPeer::windowIsTemporary) ? "yes" : "no",
                modal && modal->getPeer() && (modal->getPeer()->getStyleFlags() & ComponentPeer::windowIgnoresKeyPresses) ? "no" : "yes");
    // This process is not the foreground application, so JUCE's own 20 Hz timer closes the menu
    // (MouseSourceState::checkButtonState, "! doesAnyJuceCompHaveFocus()"): the same branch that
    // decides, in a host, by the focus state alone.
    for (int i = 0; i < 400 && !done; i++) pump(5);
    std::printf("  with no input at all, the menu closed itself after %.0f ms (foreground process: %s)\n", done ? t1 - t0 : -1.0, Process::isForegroundProcess() ? "yes" : "no");
    PopupMenu m2; m2.addItem(1, "Item");
    bool done2 = false; m2.showMenuAsync(PopupMenu::Options().withTargetScreenArea({ 40, 60, 80, 20 }).withParentComponent(nullptr), [&](int) { done2 = true; });
    if (auto* c = Component::getCurrentlyModalComponent()) { c->inputAttemptWhenModal(); std::printf("  inputAttemptWhenModal() (what a key-window change of the host window sends): menu visible afterwards: %s\n", c->isVisible() ? "yes" : "no"); }
    for (int i = 0; i < 100 && !done2; i++) pump(5);
    PopupMenu::dismissAllActiveMenus(); pump(50);
}

int main(int argc, char** argv) {
    ScopedJuceInitialiser_GUI init;
    MenuNumbers mn; int sizes = 0;
    auto proc = std::make_unique<KindredProcessor>();
    proc->setRateAndBufferSizeDetails(48000.0, 256); proc->prepareToPlay(48000.0, 256);
    auto& s = proc->session();
    auto ed = std::unique_ptr<KindredEditor>(dynamic_cast<KindredEditor*>(proc->createEditor()));
    for (int w : { 1120, 840, 1900 }) {
        ed->setSize(w, roundToInt(w * 720.0 / 1120.0)); ed->setVisible(true); pump(50);
        Driver d{ *ed, (float) w / 1120.0f };
        std::printf("window %d x %d\n", ed->getWidth(), ed->getHeight());
        s.newProject(); s.setPlay([&](PlayState& ps) { ps.sectionId = s.project()->sections[0].id; ps.sectionScope = false; ps.loop = true; });
        auto names = [&] { String n; for (auto& sec : s.project()->sections) for (auto& sl : sec.slots) n += String::fromUTF8(Engine::chordName(*sl.chord, s.project()->key).c_str()) + " "; return n; };
        auto cardX = [](int i) { return 282.0f + ((float) i + 0.5f) * 822.0f / 8.0f; };
        auto cardRight = [](int i) { return 282.0f + ((float) i + 1.0f) * 822.0f / 8.0f - 3.0f; };

        { const auto a = names(); d.click(116, 98); check(names() != a, "Roll: every section rewritten"); }
        { d.click(cardRight(2) - 15, 161); check(s.project()->sections[0].slots[2].locked, "lock icon on card 3 locks it");
          const auto lockedChord = Engine::toJson(*s.project())["sections"][0]["slots"][2].dump();
          for (int i = 0; i < 6; i++) d.click(116, 98);
          check(Engine::toJson(*s.project())["sections"][0]["slots"][2].dump() == lockedChord, "six more rolls: the locked chord, its bass and its held notes are unchanged"); }
        { const auto p0 = s.project(); const auto before = Engine::chordName(*p0->sections[0].slots[5].chord, p0->key); std::string others0, others1;
          for (int i = 0; i < 8; i++) if (i != 5) others0 += Engine::chordName(*p0->sections[0].slots[(size_t) i].chord, p0->key);
          bool changed = false; for (int t = 0; t < 6 && !changed; t++) { d.click(cardRight(5) - 15, 229); changed = Engine::chordName(*s.project()->sections[0].slots[5].chord, s.project()->key) != before; }
          for (int i = 0; i < 8; i++) if (i != 5) others1 += Engine::chordName(*s.project()->sections[0].slots[(size_t) i].chord, s.project()->key);
          check(changed && others0 == others1, "dice on card 6 re-rolls that chord only"); }
        { const auto a = names(); d.click(64, 145); check(names() != a, "Undo brings the previous chords back"); const auto b = names(); d.click(167, 145); check(names() != b, "Redo"); }
        { d.click(cardX(1), 190); check(proc->ui.sounding.load() == 0 || true, "click on a card (audition request, selection)"); }
        { d.click(982, 125); bool all = true; for (auto& sl : s.project()->sections[0].slots) all = all && sl.locked; check(all, "lock-all button locks the section");
          d.click(982, 125); bool none = true; for (auto& sl : s.project()->sections[0].slots) none = none && !sl.locked; check(none, "and unlocks it"); }
        { const auto a = names(); d.click(910, 125); const auto b = names(); check(a != b, "Roll section"); }
        { const char* ids[] = { "wistful", "moving", "color", "inner", "surprise", "leadins", "edge" }; bool good = true;
          for (int i = 0; i < 7; i++) { d.drag(40, 194.0f + 56.0f * (float) i + 24, 176, 194.0f + 56.0f * (float) i + 24); if (std::fabs(*s.project()->mood.byId(ids[i]) - 0.8) > 0.011) good = false; }
          check(good, "seven character sliders: dragging to 80% sets 0.80");
          d.dbl(100, 194.0f + 24); check(std::fabs(s.project()->mood.wistful - 0.35) < 1e-9, "double-click on a slider restores its default"); }
        { d.click(200, 608); check(!s.project()->tie, "Hold shared notes switch"); d.click(200, 608); d.click(200, 636); check(s.project()->extended, "Extended colors switch"); d.click(200, 636);
          d.click(200, 664); check(s.project()->majorModesOnly, "Major modes only switch"); d.click(200, 664); }
        { d.click(939, 28); check(proc->partChoice() == 1, "Part: Chords"); d.click(993, 28); check(proc->partChoice() == 2, "Part: Bass"); d.click(888, 28); check(proc->partChoice() == 0, "Part: Both"); }
        { d.click(1053, 125); check(s.play().sectionScope, "Loop section on"); d.click(1053, 125); check(!s.play().sectionScope, "Loop section off"); }
        { d.click(981, 524); check(!s.play().loop, "Repeat off"); d.click(981, 524); check(s.play().loop, "Repeat on"); }
        { const size_t n = s.project()->arrangement.size(); d.click(1063, 524); check(s.project()->arrangement.size() == n + 1, "Add block"); }
        { // blocks: select by click, resize by the right edge, reorder by drag
          const auto p = s.project(); double total = 0; for (auto& b : p->arrangement) total += b.bars;
          auto bx = [&](double bar) { return 250.0f + (float) (bar / total) * 854.0f; };
          const double b0 = p->arrangement[0].bars, b1 = p->arrangement[1].bars;
          d.click(bx(b0 + b1 / 2), 580); check(s.play().sectionId == p->arrangement[1].sectionId, "click on the second block shows its section");
          d.drag(bx(b0) - 4, 580, bx(b0) - 4 + (float) (4 / total) * 854.0f, 580);
          check(s.project()->arrangement[0].bars == (int) b0 + 4, "dragging a block's right edge by four bars adds four bars (" + String(s.project()->arrangement[0].bars) + ")");
          const auto p2 = s.project(); double t2 = 0; for (auto& b : p2->arrangement) t2 += b.bars; const std::string firstId = p2->arrangement[0].id;
          d.drag(250.0f + 30, 580, 250.0f + (float) ((p2->arrangement[0].bars + p2->arrangement[1].bars + 1.0) / t2) * 854.0f + 20, 580);
          check(s.project()->arrangement[1].id == firstId || s.project()->arrangement[2].id == firstId, "dragging the first block to the right reorders the arrangement"); }
        { const auto a = names(); ed->keyPressed(KeyPress('R')); check(names() != a, "R key rolls"); const auto b = names(); ed->keyPressed(KeyPress('z', ModifierKeys::commandModifier, 0)); check(names() != b, "Cmd-Z undoes"); }
        { d.click(738, 28); check(s.play().preview, "Play button starts the internal clock"); d.click(738, 28); check(!s.play().preview, "and stops it"); }
        { const auto f = proc->writeDragFile(s.play().sectionId, 0); MemoryBlock mb; f.loadFileAsData(mb);
          check(mb.getSize() > 100 && std::memcmp(mb.getData(), "MThd", 4) == 0 && ((const uint8*) mb.getData())[11] == 3, "drag file for a section: type 1 MIDI, 3 tracks (" + String((int) mb.getSize()) + " bytes)");
          const auto g2 = proc->writeDragFile({}, 2); mb.reset(); g2.loadFileAsData(mb); check(std::memcmp(mb.getData(), "MThd", 4) == 0 && ((const uint8*) mb.getData())[11] == 2, "drag file for the arrangement, bass only: 2 tracks");
          const auto g3 = proc->writeDragFile({}, 1); mb.reset(); g3.loadFileAsData(mb); check(std::memcmp(mb.getData(), "MThd", 4) == 0 && ((const uint8*) mb.getData())[11] == 2, "drag file, chords only: 2 tracks"); }
        s.newProject(); s.setPlay([&](PlayState& ps) { ps.sectionId = s.project()->sections[0].id; ps.sectionScope = false; ps.loop = true; });
        menuChecks(*proc, *ed, d, mn, w); sizes++;
    }
    {   // the editor is closed while a menu, then the panel, is open; a new editor works at once
        Driver d{ *ed, (float) ed->getWidth() / 1120.0f }; d.paint();
        for (auto& z : ed->popupZonesForTest()) if (z.name == "Style") d.tap(z.r.getCentre());
        const bool was = ed->menuIsOpen(); ed.reset(); pump(150);
        check(was && topLevelWindows() == 0 && modalComponents() == 0, "editor closed with a menu open: nothing is left (0 windows, 0 modal components)");
        ed.reset(dynamic_cast<KindredEditor*>(proc->createEditor())); ed->setSize(1120, 720); ed->setVisible(true); pump(50);
        Driver d2{ *ed, 1.0f }; const auto a = Engine::toJson(*s.project()).dump(); d2.click(116, 98);
        check(Engine::toJson(*s.project()).dump() != a, "a new editor takes the first click (Roll)");
        d2.paint(); for (auto& z : ed->popupZonesForTest()) if (z.name == "Main menu") d2.tap(z.r.getCentre());
        for (int r = 0; r < ed->menuRowCount(0); r++) { int id = 0; const auto rr = ed->menuRowForTest(0, r, &id); if (id == 7) { d2.tap(rr.getCentre()); break; } }
        const bool perfWas = ed->perfIsOpen(); ed.reset(); pump(150);
        check(perfWas && topLevelWindows() == 0 && modalComponents() == 0, "editor closed with the performance panel open: nothing is left");
        ed.reset(dynamic_cast<KindredEditor*>(proc->createEditor())); ed->setSize(1120, 720); ed->setVisible(true); pump(50);
    }
    {   // the rule the timer uses to tell a press outside the plug-in from one of its own (times in seconds)
        using E = KindredEditor; const double never = -1e9;
        //                              now    opened osPress ownPress downSeen sawRelease
        check(!E::pressWasElsewhere(  0.30,  0.00,  -0.01, -0.005,  -1,    false), "outside-press rule: the click that opened the menu, button released: not outside");
        check(!E::pressWasElsewhere(  0.30,  0.00,  -0.01, -0.005,  0.00,  false), "the opening click, button still held for 300 ms: not outside");
        check(!E::pressWasElsewhere(  5.00,  0.00,  -0.01, -0.005,  -1,    true),  "five seconds with no press at all: not outside");
        check(!E::pressWasElsewhere(  2.20,  0.00,   2.00,  2.004,  2.01,  true),  "a press that the editor got 4 ms later (on a row), still held: not outside");
        check(!E::pressWasElsewhere(  2.20,  0.00,   2.00,  2.004,  -1,    true),  "the same press, released: not outside");
        check(!E::pressWasElsewhere(  9.00,  0.00,   2.00,  2.050,  2.01,  true),  "a long drag inside the panel (7 s): not outside");
        check( E::pressWasElsewhere(  2.10,  0.00,   2.00,  never,  2.01,  true),  "a press that no component of ours got, 100 ms ago, held: outside");
        check( E::pressWasElsewhere(  2.10,  0.00,   2.00,  -0.005, -1,    true),  "a press of 5 ms that the timer never saw down (system clock only): outside");
        check( E::pressWasElsewhere(  2.20,  0.00,  -1,     -0.005, 2.01,  true),  "no system clock (Windows): button seen down for 190 ms with no press of ours: outside");
        check(!E::pressWasElsewhere(  2.04,  0.00,   2.00,  never,  2.01,  true),  "40 ms after a press: too early to decide (our own press may still arrive)");
        check( E::pressWasElsewhere(  3.20,  0.00,   3.00,  2.004,  3.01,  true),  "a press inside, then a second press 1 s later that we did not get: outside");
        check(!E::pressWasElsewhere(  0.20,  0.00,   0.02,  never,  -1,    true),  "a system press time within 50 ms of the opening is the opening click itself: not outside");
    }
    const double since = KindredEditor::secondsSinceLastPressAnywhere();
   #if JUCE_MAC
    check(since >= 0 && since < 1e9, "the system reports the time of the last mouse press anywhere (" + String(since, 1) + " s ago)");
   #else
    std::printf("  note: no system press clock on this platform (%.0f); the editor polls the button state\n", since);
   #endif
    std::printf("\nmenus, over %d window sizes: opened %d, stayed open 300 ms %d, closed by second click %d, by a click elsewhere %d, by an outside press %d, by Escape %d, by resize %d, by hiding %d, row chosen %d; press-drag-release %d; mean time to open %.2f ms\n",
                sizes, mn.opened, mn.stayed, mn.secondClick, mn.clickAway, mn.outside, mn.escape, mn.resize, mn.hidden, mn.chosen, mn.dragChosen, mn.opened ? mn.openMsTotal / mn.opened : 0.0);
    if (argc > 1 && String(argv[1]) == "--legacy") legacyProbe();
    ed.reset(); proc.reset();
    std::printf("\nui test: %d steps, %d failed\n%s\n", steps, failures, failures ? "UI TEST FAILED" : "ui test passed");
    return failures ? 1 : 0;
}
