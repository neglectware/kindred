// The shared state behind one or more plug-in instances.
//
// Instances with the same link letter in one host process hold the same
// Session, so two instances on two tracks (one playing the chords, one the
// bass) always show and play the same progression. Each instance still saves
// the whole project in its own state; on load, the newest copy wins.
//
// Threading: every edit makes a new immutable Project snapshot under a mutex.
// Readers take the current snapshot pointer and never see a half-made change.
#pragma once
#include "../engine/Engine.h"
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

namespace kindred {

// Built-in styles plus any .json files in the user's style folder.
class StyleLibrary {
public:
    static StyleLibrary& get();
    struct Entry { StylePtr style; bool builtIn = true; std::string path; };
    std::vector<Entry> all() const;
    StylePtr find(const std::string& id) const;        // falls back to the first built-in style
    StylePtr first() const;
    void addBuiltIn(const char* jsonText, size_t size);
    // Reads every .json in `folder`; returns one line per file that could not be used.
    std::vector<std::string> scanFolder(const std::string& folder, const std::function<std::vector<std::pair<std::string, std::string>>(const std::string&)>& listFiles);
private:
    mutable std::mutex mx;
    std::vector<Entry> entries;
};

struct PlayState {
    bool sectionScope = false;        // false: play the arrangement; true: loop one section
    std::string sectionId;            // the selected section (also the one that loops)
    bool loop = true;                 // repeat the arrangement when it ends
    int startBar = 1;                 // host bar where the arrangement begins
    bool followMeter = true;          // take the meter from the host
    bool preview = false;             // the plug-in's own transport is running
    double previewStartMs = 0;        // high-resolution clock time when it started
    uint64_t previewGen = 0;
};
struct Stamp { std::string lineage; uint64_t edits = 0; double timeMs = 0; };

class Session {
public:
    struct Listener { virtual ~Listener() = default; virtual void sessionChanged() = 0; };   // called on the editing thread

    // Instances that pass the same non-empty link letter get the same Session.
    static std::shared_ptr<Session> join(const std::string& link);
    Session();

    std::shared_ptr<const Project> project() const { std::lock_guard<std::recursive_mutex> l(mx); return proj; }
    Engine engine() const { std::lock_guard<std::recursive_mutex> l(mx); return Engine(style); }
    StylePtr currentStyle() const { std::lock_guard<std::recursive_mutex> l(mx); return style; }
    PlayState play() const { std::lock_guard<std::recursive_mutex> l(mx); return playState; }
    uint64_t revision() const { std::lock_guard<std::recursive_mutex> l(mx); return rev; }
    Stamp stamp() const { std::lock_guard<std::recursive_mutex> l(mx); return st; }
    std::string lastAction() const { std::lock_guard<std::recursive_mutex> l(mx); return action; }

    // Changes the project. With `undoable`, the state before the change goes on the undo stack.
    void edit(const std::function<void(Project&, const Engine&)>& fn, bool undoable = true, const std::string& name = {});
    void setPlay(const std::function<void(PlayState&)>& fn);
    bool canUndo() const { std::lock_guard<std::recursive_mutex> l(mx); return !undoStack.empty(); }
    bool canRedo() const { std::lock_guard<std::recursive_mutex> l(mx); return !redoStack.empty(); }
    void undo(); void redo();

    // High-level actions used by the editor.
    void rollAll();
    void rollSection(const std::string& sectionId);
    void rerollSlot(const std::string& sectionId, int slot);
    void setStyle(const std::string& styleId, bool reroll);
    void newProject();
    void importProject(const Json& file);              // a saved project (any format); throws on a bad file
    std::string newId();
    uint32_t newSeed();

    // Saved state: the project, the play settings and the stamp.
    Json saveState() const;
    // Takes a saved state if this session is still untouched or the state is newer than what it holds.
    void restoreState(const Json& state);

    void addListener(Listener* l); void removeListener(Listener* l);
    int memberCount() const { std::lock_guard<std::recursive_mutex> l(mx); return (int) listeners.size(); }
    std::vector<Listener*> members() const { std::lock_guard<std::recursive_mutex> l(mx); return listeners; }

private:
    void commit(Project&& next, bool undoable, const std::string& name);
    void notify();
    mutable std::recursive_mutex mx;
    std::shared_ptr<const Project> proj;
    StylePtr style;
    PlayState playState;
    Stamp st;
    uint64_t rev = 1;
    bool pristine = true;                 // nothing restored and nothing edited by the user yet
    std::string action;
    std::vector<std::shared_ptr<const Project>> undoStack, redoStack;
    std::vector<Listener*> listeners;
    std::mt19937 rng;
};

}  // namespace kindred
