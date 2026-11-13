// The Kindred editor. One window, drawn as one picture at a base size of
// 1120 x 720 and scaled as a whole, so it is the same at every size.
#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include <unordered_map>
#include "../plugin/PluginProcessor.h"

namespace kindred {

struct Theme {
    juce::Colour bg, panel, card, cardHi, line, text, dim, faint, accent, onAccent, playhead, thread;
    juce::Colour tonic, sub, dom, med, borrowed, approach;
    bool light = false;
    static Theme dark();
    static Theme lightTheme();
    juce::Colour function(char f) const;
    juce::Colour role(const std::string& role) const;
};

class KindredEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer {
public:
    explicit KindredEditor(KindredProcessor&);
    ~KindredEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;

    // For the screenshot tool and tests.
    void setLightTheme(bool light);
    void setHoverForTest(juce::Point<float> basePoint) { hoverPt = basePoint; updateHover(); repaint(); }
    double lastPaintMs = 0;
    // Menus and the performance panel, for tests (rectangles are in base coordinates).
    struct TestZone { juce::String name; juce::Rectangle<float> r; bool rightClick = false; };
    std::vector<TestZone> popupZonesForTest() const;                // every control that opens a menu or the panel
    bool menuIsOpen() const { return menu != nullptr; }
    int menuPanelCount() const { return menu ? (int) menu->panels.size() : 0; }
    int menuRowCount(int panel) const;
    juce::Rectangle<float> menuRowForTest(int panel, int row, int* id = nullptr, bool* enabled = nullptr, bool* hasSub = nullptr) const;
    juce::Rectangle<float> menuBoundsForTest(int panel) const;
    bool perfIsOpen() const { return perf != nullptr; }
    juce::TextEditor* nameEditorForTest() const { return nameEdit && !nameEditDone ? nameEdit.get() : nullptr; }
    juce::Rectangle<float> perfBoundsForTest() const;
    void outsidePressForTest() { outsidePress(); }                  // what the timer does when it sees a press outside the editor
    static double secondsSinceLastPressAnywhere();                  // the operating system's answer; < 0 when it has none
    static bool pressWasElsewhere(double now, double openedAt, double osPressAt, double ownPressAt, double downSeenAt, bool sawRelease);
    int menusOpened = 0, menusChosen = 0, popupsDismissed = 0;
    static constexpr int baseW = 1120, baseH = 720;

private:
    enum Kind { None, Roll, Undo, Redo, SliderZ, Toggle, StylePill, KeyPill, ModePill, MeterPill, TempoPill, Play, Part, LinkPill, MenuBtn, Tab, TabAdd,
                SecRole, SecLen, SecBars, SecPlan, SecRoll, SecLock, Scope, Card, CardLock, CardDice, BlockZ, BlockEdge, BlockAdd, LoopToggle, DragSection, DragArr, Keyboard };
    struct Zone { juce::Rectangle<float> r; Kind kind = None; int a = 0; juce::String hint; };
    struct DNote { double t0, t1; int pitch; bool bass; int slot; bool shared; };
    struct Link { double t; int from, to, slotFrom, slotTo; };       // one voice moving from one pitch to the next at a chord change
    struct SectionView { std::string id; uint64_t rev = 0; std::vector<DNote> notes; std::vector<Link> links; std::vector<double> slotStart; double total = 0; };

    void timerCallback() override;
    void paintBase(juce::Graphics&);
    void paintHeader(juce::Graphics&, const Project&);
    void paintRail(juce::Graphics&, const Project&);
    void paintTabs(juce::Graphics&, const Project&);
    void paintStage(juce::Graphics&, const Project&);
    void paintArrangement(juce::Graphics&, const Project&);
    void paintFooter(juce::Graphics&, const Project&);
    void zone(juce::Rectangle<float> r, Kind k, int a, const juce::String& hint) { zones.push_back({ r, k, a, hint }); }
    const Zone* zoneAt(juce::Point<float> p) const;
    juce::Point<float> toBase(juce::Point<float> p) const { return p / scale; }
    void updateHover();
    // Menus are drawn by the editor inside its own window. They are not separate windows and
    // nothing is modal, so the host's window focus cannot open, close or strand one (NOTES.md).
    struct MenuEntry { int id = 0; juce::String text; bool enabled = true, ticked = false, separator = false; std::vector<MenuEntry> sub; };
    struct MenuPanel { std::vector<MenuEntry> items; juce::Rectangle<float> r; std::vector<juce::Rectangle<float>> rows; int hover = -1, openRow = -1; };
    struct MenuState { std::vector<MenuPanel> panels; std::function<void(int)> fn; juce::uint32 openedAt = 0; bool openerPress = false; juce::Point<float> openerPt; };
    std::unique_ptr<MenuState> menu;
    std::unique_ptr<juce::Component> perf;                 // the performance panel, a child of the editor
    std::unique_ptr<juce::TextEditor> nameEdit; std::string nameEditId; bool nameEditDone = false;   // rename a section in place
    void finishRename(bool keep);
    double popupOpenedAt = 0, popupDownSeenAt = -1, ownPressAt = -1e9, closedByOutsideAt = -1e9; bool popupSawRelease = false; bool swallowPress = false;
    struct PressTap : juce::MouseListener { KindredEditor& owner; explicit PressTap(KindredEditor& o) : owner(o) {} void mouseDown(const juce::MouseEvent&) override; };
    PressTap pressTap{ *this };
    void showMenu(juce::PopupMenu& m, juce::Rectangle<float> baseRect, std::function<void(int)> fn);
    void layoutMenuPanel(MenuPanel&, juce::Rectangle<float> anchor, bool asSubmenu) const;
    void paintMenu(juce::Graphics&);
    bool menuHit(juce::Point<float> p, int& panel, int& row) const;
    void menuHover(juce::Point<float> p);
    void menuOpenSub(int panel, int row);
    void menuChoose(int panel, int row);
    bool menuKey(const juce::KeyPress&);
    void closePopups();
    void outsidePress();
    void watchOutsidePress();
    void openMainMenu(juce::Rectangle<float> r);
    void openPerformance(juce::Rectangle<float> r);
    void renameSection(const std::string& id);
    void tabMenu(int index, juce::Rectangle<float> r);
    void cardMenu(int slot, juce::Rectangle<float> r);
    void blockMenu(int index, juce::Rectangle<float> r);
    void sectionSetting(Kind k, juce::Rectangle<float> r);
    void rollSectionIfFree(const std::string& id);
    void selectSection(const std::string& id, bool user);
    void buildView(const Project&, const Section&);
    void say(const juce::String& s) { status = s; statusTime = juce::Time::getMillisecondCounter(); repaint(); }
    const Section* selected(const Project& p) const;
    double localPos(const Project& p, bool& inSelected) const;
    struct F { int which; float h; };                      // a face (0 regular, 1 medium, 2 semibold, 3 display, 4 serif) and a height
    struct Run { juce::GlyphArrangement ga; float w = 0; };
    F font(int which, float h) const { return { which, h }; }
    const juce::Font& realFont(F) const;
    const Run& run(const juce::String&, F) const;          // shaped once, then drawn from the cache
    float text(juce::Graphics&, const juce::String&, float x, float baseline, F, juce::Colour, int align = 0) const;
    float textWidth(const juce::String&, F) const;
    juce::Image baseImage; bool baseLight = false;        // the static background, drawn once per size and theme
    mutable std::map<int, juce::Font> fontCache;
    mutable std::unordered_map<std::string, Run> runCache;
    float chordSymbol(juce::Graphics*, const Project&, const Chord&, float x, float baseline, float size, juce::Colour main, juce::Colour sub, int align) const;

    KindredProcessor& proc;
    Theme th;
    float scale = 1;
    std::vector<Zone> zones;
    juce::Point<float> hoverPt{ -1, -1 };
    Kind hoverKind = None; int hoverA = -1; juce::String hoverHint;
    Kind dragKind = None; int dragA = -1; float dragStartX = 0; int dragStartBars = 0; bool dragMoved = false; int dropIndex = -1;
    int selSlot = -1;
    bool follow = true;
    uint64_t seenRev = 0;
    SectionView view;
    juce::String status; juce::uint32 statusTime = 0;
    juce::uint32 flashTime = 0; std::vector<std::string> lastCardText; std::vector<juce::uint32> cardFlash;
    juce::Typeface::Ptr tfRegular, tfMedium, tfSemi, tfDisplay, tfSerif;
    std::unique_ptr<juce::FileChooser> chooser;
    bool wasPlaying = false; double lastPos = -1; int lastSounding = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KindredEditor)
};

}  // namespace kindred
