#include "Session.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <random>
#include <utility>
#include <map>

namespace kindred {

static double nowMs() { return (double) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }

// ---------------------------------------------------------------- styles ----
StyleLibrary& StyleLibrary::get() { static StyleLibrary lib; return lib; }
std::vector<StyleLibrary::Entry> StyleLibrary::all() const { std::lock_guard<std::mutex> l(mx); return entries; }
StylePtr StyleLibrary::first() const { std::lock_guard<std::mutex> l(mx); return entries.empty() ? nullptr : entries.front().style; }
StylePtr StyleLibrary::find(const std::string& id) const {
    std::lock_guard<std::mutex> l(mx);
    for (auto& e : entries) if (e.style->id == id) return e.style;
    return entries.empty() ? nullptr : entries.front().style;
}
void StyleLibrary::addBuiltIn(const char* jsonText, size_t size) {
    auto st = Style::fromText(std::string(jsonText, size));       // a broken built-in style is a build error: let it throw
    std::lock_guard<std::mutex> l(mx);
    for (auto& e : entries) if (e.style->id == st->id) return;
    entries.push_back({ st, true, {} });
}
std::vector<std::string> StyleLibrary::scanFolder(const std::string& folder,
        const std::function<std::vector<std::pair<std::string, std::string>>(const std::string&)>& listFiles) {
    std::vector<std::string> problems;
    for (auto& f : listFiles(folder)) {            // (path, text)
        try {
            auto st = Style::fromText(f.second);
            std::lock_guard<std::mutex> l(mx);
            bool replaced = false;
            for (auto& e : entries) if (e.style->id == st->id) { if (!e.builtIn) { e.style = st; e.path = f.first; } else problems.push_back(f.first + ": the id \"" + st->id + "\" belongs to a built-in style"); replaced = true; }
            if (!replaced) entries.push_back({ st, false, f.first });
        } catch (const std::exception& e) { problems.push_back(f.first + ": " + e.what()); }
    }
    return problems;
}

// --------------------------------------------------------------- registry ----
namespace {
struct Registry { std::mutex mx; std::map<std::string, std::weak_ptr<Session>> groups; };
Registry& registry() { static Registry r; return r; }
}
std::shared_ptr<Session> Session::join(const std::string& link) {
    if (link.empty()) return std::make_shared<Session>();
    auto& r = registry();
    std::lock_guard<std::mutex> l(r.mx);
    if (auto s = r.groups[link].lock()) return s;
    auto s = std::make_shared<Session>();
    r.groups[link] = s;
    return s;
}

Session::Session() : rng(std::random_device{}()) {
    style = StyleLibrary::get().first();
    st.lineage = newId() + newId(); st.timeMs = nowMs();
    Engine eng(style);
    Project p = eng.newProject([this] { return newId(); });
    eng.rollAll(p, [this] { return newSeed(); });
    if (!p.sections.empty()) playState.sectionId = p.sections.front().id;
    proj = std::make_shared<const Project>(std::move(p));
}
std::string Session::newId() {
    static const char* a = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::lock_guard<std::recursive_mutex> l(mx);
    std::string s; for (int i = 0; i < 7; i++) s += a[rng() % 36];
    return s;
}
uint32_t Session::newSeed() { std::lock_guard<std::recursive_mutex> l(mx); return (uint32_t) rng(); }

void Session::addListener(Listener* l) { std::lock_guard<std::recursive_mutex> g(mx); if (std::find(listeners.begin(), listeners.end(), l) == listeners.end()) listeners.push_back(l); }
void Session::removeListener(Listener* l) { std::lock_guard<std::recursive_mutex> g(mx); listeners.erase(std::remove(listeners.begin(), listeners.end(), l), listeners.end()); }
void Session::notify() {
    std::vector<Listener*> ls;
    { std::lock_guard<std::recursive_mutex> g(mx); ls = listeners; }
    for (auto* l : ls) l->sessionChanged();
}
void Session::commit(Project&& next, bool undoable, const std::string& name) {
    {
        std::lock_guard<std::recursive_mutex> g(mx);
        if (undoable) { undoStack.push_back(proj); if (undoStack.size() > 100) undoStack.erase(undoStack.begin()); redoStack.clear(); pristine = false; action = name; }
        if (next.style != style->id) { style = StyleLibrary::get().find(next.style); next.style = style->id; }
        // Keep the selected section valid.
        if (!next.section(playState.sectionId)) playState.sectionId = next.sections.empty() ? std::string() : next.sections.front().id;
        proj = std::make_shared<const Project>(std::move(next));
        rev++; st.edits++; st.timeMs = nowMs();
    }
    notify();
}
void Session::edit(const std::function<void(Project&, const Engine&)>& fn, bool undoable, const std::string& name) {
    Project next; StylePtr s;
    { std::lock_guard<std::recursive_mutex> g(mx); next = *proj; s = style; }
    fn(next, Engine(s));
    commit(std::move(next), undoable, name);
}
void Session::setPlay(const std::function<void(PlayState&)>& fn) {
    { std::lock_guard<std::recursive_mutex> g(mx); fn(playState); rev++; }
    notify();
}
void Session::undo() {
    { std::lock_guard<std::recursive_mutex> g(mx);
      if (undoStack.empty()) return;
      redoStack.push_back(proj); proj = undoStack.back(); undoStack.pop_back();
      style = StyleLibrary::get().find(proj->style);
      if (!proj->section(playState.sectionId)) playState.sectionId = proj->sections.empty() ? std::string() : proj->sections.front().id;
      rev++; st.edits++; st.timeMs = nowMs(); action = "Undo"; }
    notify();
}
void Session::redo() {
    { std::lock_guard<std::recursive_mutex> g(mx);
      if (redoStack.empty()) return;
      undoStack.push_back(proj); proj = redoStack.back(); redoStack.pop_back();
      style = StyleLibrary::get().find(proj->style);
      if (!proj->section(playState.sectionId)) playState.sectionId = proj->sections.empty() ? std::string() : proj->sections.front().id;
      rev++; st.edits++; st.timeMs = nowMs(); action = "Redo"; }
    notify();
}

void Session::rollAll() {
    edit([this](Project& p, const Engine& e) { e.rollAll(p, [this] { return newSeed(); }); }, true, "Roll all");
}
void Session::rollSection(const std::string& id) {
    edit([&](Project& p, const Engine& e) { if (auto* s = p.section(id)) e.rollSection(p, *s, nullptr, newSeed()); }, true, "Roll section");
}
void Session::rerollSlot(const std::string& id, int slot) {
    edit([&](Project& p, const Engine& e) { if (auto* s = p.section(id)) { std::vector<int> only{ slot }; e.rollSection(p, *s, &only, newSeed()); } }, true, "Re-roll chord");
}
void Session::setStyle(const std::string& styleId, bool reroll) {
    StylePtr next = StyleLibrary::get().find(styleId);
    if (!next) return;
    Project p; { std::lock_guard<std::recursive_mutex> g(mx); p = *proj; }
    p.style = next->id;
    Engine e(next);
    // Keep what still means the same thing in the new style; clamp the controls to its ranges.
    if (!next->mode(p.mode)) p.mode = next->defaultMode;
    for (auto& sp : next->spectra) if (double* v = p.mood.byId(sp.id)) *v = std::max(sp.min, std::min(sp.max, *v));
    for (auto& s : p.sections) { if (!next->role(s.role)) s.role = "home"; if (s.planPref != "auto" && !next->plan(s.planPref)) s.planPref = "auto"; }
    { std::lock_guard<std::recursive_mutex> g(mx); style = next; }
    if (reroll) e.rollAll(p, [this] { return newSeed(); });
    commit(std::move(p), true, "Style: " + next->name);
}
void Session::newProject() {
    StylePtr s = currentStyle();
    Engine e(s);
    Project p = e.newProject([this] { return newId(); });
    e.rollAll(p, [this] { return newSeed(); });
    commit(std::move(p), true, "New project");
}
void Session::importProject(const Json& file) {
    Project p = Engine::fromJson(file.has("project") && file["project"].isObject() ? file["project"] : file);
    StylePtr s = StyleLibrary::get().find(p.style);
    { std::lock_guard<std::recursive_mutex> g(mx); style = s; }
    p.style = s->id;
    commit(std::move(p), true, "Open project");
}

Json Session::saveState() const {
    std::lock_guard<std::recursive_mutex> g(mx);
    Json j = Json::object();
    j.set("project", Engine::toJson(*proj));
    Json pl = Json::object();
    pl.set("sectionScope", playState.sectionScope); pl.set("sectionId", playState.sectionId); pl.set("loop", playState.loop);
    pl.set("startBar", playState.startBar); pl.set("followMeter", playState.followMeter);
    j.set("play", pl);
    Json s = Json::object(); s.set("lineage", st.lineage); s.set("edits", (double) st.edits); s.set("time", st.timeMs);
    j.set("stamp", s);
    return j;
}
void Session::restoreState(const Json& state) {
    if (!state["project"].isObject()) return;
    Stamp in; in.lineage = state["stamp"]["lineage"].stringOr(""); in.edits = (uint64_t) state["stamp"]["edits"].number(0); in.timeMs = state["stamp"]["time"].number(0);
    Project p;
    try { p = Engine::fromJson(state["project"]); } catch (const std::exception&) { return; }
    {
        std::lock_guard<std::recursive_mutex> g(mx);
        bool take = pristine;
        if (!take) {
            if (!in.lineage.empty() && in.lineage == st.lineage) take = in.edits > st.edits;
            else take = listeners.size() <= 1 || in.timeMs > st.timeMs;
        }
        if (!take) return;
        style = StyleLibrary::get().find(p.style); p.style = style->id;
        const Json& pl = state["play"];
        playState.sectionScope = pl["sectionScope"].boolean(); playState.sectionId = pl["sectionId"].stringOr("");
        playState.loop = pl.has("loop") ? pl["loop"].boolean() : true; playState.startBar = std::max(1, pl["startBar"].integer(1));
        playState.followMeter = pl.has("followMeter") ? pl["followMeter"].boolean() : true;
        playState.preview = false;
        if (!p.section(playState.sectionId)) playState.sectionId = p.sections.empty() ? std::string() : p.sections.front().id;
        proj = std::make_shared<const Project>(std::move(p));
        undoStack.clear(); redoStack.clear();
        if (!in.lineage.empty()) st = in;
        pristine = false; rev++;
    }
    notify();
}

}  // namespace kindred
