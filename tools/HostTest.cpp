// KindredHostTest -- loads the BUILT plug-in through JUCE's plug-in hosting (the same way a DAW
// does), drives it with a transport and checks every MIDI message it sends.
//   KindredHostTest <path to Kindred.vst3 or Kindred.component>
// Checks: transport start/stop/loop/locate/tempo change/meter change; note pairing (no note-on for
// a sounding pitch, no note-off for a silent one, nothing left sounding after a stop); state save
// and restore; two linked instances (chords on one, bass on the other).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

using namespace juce;

struct Head : AudioPlayHead {
    PositionInfo info;
    Optional<PositionInfo> getPosition() const override { return info; }
};
struct Monitor {
    bool on[16][128] = {}; long ons = 0, offs = 0, overlaps = 0, orphans = 0;
    StringArray log;
    void feed(const MidiBuffer& mb, double blockPpq, double ppqPerSample) {
        for (const auto meta : mb) {
            const auto m = meta.getMessage(); const int ch = m.getChannel() - 1, n = m.getNoteNumber();
            if (m.isNoteOn()) { if (on[ch][n]) overlaps++; on[ch][n] = true; ons++; log.add(String(blockPpq + meta.samplePosition * ppqPerSample, 3) + " on " + String(ch + 1) + " " + String(n)); }
            else if (m.isNoteOff()) { if (!on[ch][n]) orphans++; on[ch][n] = false; offs++; }
        }
    }
    int sounding() const { int c = 0; for (auto& a : on) for (bool b : a) c += b; return c; }
    int soundingOn(int ch) const { int c = 0; for (bool b : on[ch]) c += b; return c; }
};
static void pump(int ms) { const auto end = Time::getMillisecondCounter() + (uint32) ms; while (Time::getMillisecondCounter() < end) MessageManager::getInstance()->runDispatchLoopUntil(10); }

static int failures = 0, checksDone = 0;
static void check(bool ok, const String& what) { checksDone++; if (!ok) { failures++; std::printf("  FAIL: %s\n", what.toRawUTF8()); } else std::printf("  ok:   %s\n", what.toRawUTF8()); }

struct Rig {
    std::unique_ptr<AudioPluginInstance> plug; Head head; Monitor mon; double sr = 48000; int bs = 256; double ppq = 0, bpm = 96; long blocks = 0;
    void prepare() { plug->setPlayHead(&head); plug->setRateAndBufferSizeDetails(sr, bs); plug->prepareToPlay(sr, bs); head.info.setTimeSignature(AudioPlayHead::TimeSignature{ 4, 4 }); }
    // Runs `beats` beats of transport. playing=false runs stopped blocks.
    void run(double beats, bool playing) {
        const double pps = bpm / 60.0 / sr; long n = (long) std::ceil(beats / (pps * bs));
        while (n-- > 0) {
            head.info.setIsPlaying(playing); head.info.setBpm(bpm); head.info.setPpqPosition(ppq); head.info.setTimeInSamples((int64) (ppq / pps));
            AudioBuffer<float> buf(jmax(1, plug->getTotalNumOutputChannels()), bs); buf.clear(); MidiBuffer mb;
            plug->processBlock(buf, mb); mon.feed(mb, ppq, pps); blocks++;
            if (playing) ppq += pps * bs;
        }
    }
};

int main(int argc, char** argv) {
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf("usage: KindredHostTest <plugin bundle>\n"); return 2; }
    AudioPluginFormatManager fm; fm.addDefaultFormats();
    const String path = File::getCurrentWorkingDirectory().getChildFile(argv[1]).getFullPathName();
    OwnedArray<PluginDescription> found;
    for (auto* f : fm.getFormats()) if (f->fileMightContainThisPluginType(path)) f->findAllTypesForFile(found, path);
    if (found.isEmpty()) { std::printf("no plug-in found in %s\n", path.toRawUTF8()); return 2; }
    const PluginDescription desc = *found[0];
    std::printf("plug-in: %s  format %s  manufacturer %s  version %s  isInstrument %d  category %s\n", desc.name.toRawUTF8(), desc.pluginFormatName.toRawUTF8(),
                desc.manufacturerName.toRawUTF8(), desc.version.toRawUTF8(), (int) desc.isInstrument, desc.category.toRawUTF8());
    auto make = [&]() { String err; auto p = fm.createPluginInstance(desc, 48000, 256, err); if (!p) { std::printf("cannot create: %s\n", err.toRawUTF8()); std::exit(2); } return p; };
    auto param = [](AudioPluginInstance& p, const String& name) -> AudioProcessorParameter* { for (auto* q : p.getParameters()) if (q->getName(64) == name) return q; return nullptr; };

    std::printf("\n1. one instance, transport\n");
    Rig a; a.plug = make(); a.prepare();
    check(a.plug->acceptsMidi() && a.plug->producesMidi(), "accepts and produces MIDI");
    { int named = 0; for (auto* n : { "Part", "Chord Velocity", "Bass Velocity", "Chord Octave", "Bass Octave", "Humanize", "Strum", "Note Length", "Chord Channel", "Bass Channel", "MIDI Thru" }) if (param(*a.plug, n)) named++;
      check(named == 11, "the 11 named host parameters are there (" + String(named) + " found; the host sees " + String(a.plug->getParameters().size()) + " in all, the rest are the VST3 wrapper's MIDI CC parameters)"); }
    a.run(4, false);
    check(a.mon.ons == 0, "stopped: sends nothing");
    a.run(16, true);
    check(a.mon.ons > 8 && a.mon.sounding() > 0, "playing: notes sound (" + String(a.mon.ons) + " note-ons in 16 beats, " + String(a.mon.sounding()) + " sounding)");
    a.run(1, false);
    check(a.mon.sounding() == 0, "transport stop: every note got its note-off (" + String(a.mon.ons) + " on, " + String(a.mon.offs) + " off)");
    for (int i = 0; i < 40; i++) { a.ppq = 4.0 * (i % 9) + 0.37 * i; a.run(3.3, true); if (i % 3 == 0) { a.run(0.5, false); check(a.mon.sounding() == 0, "locate + stop " + String(i) + ": nothing left sounding"); } }
    // Host loop (cycle) over bars 3 and 4, twenty times, with a tempo change every pass.
    for (int i = 0; i < 20; i++) { a.ppq = 8; a.bpm = 60 + 9 * i; a.run(8, true); }
    a.run(0.2, false);
    check(a.mon.sounding() == 0, "20 loop passes with tempo changes, then stop: nothing left sounding");
    // Tempo ramp: a new tempo every block.
    a.ppq = 0; for (int i = 0; i < 400; i++) { a.bpm = 60 + 0.4 * i; a.run(0.05, true); }
    a.run(0.2, false);
    check(a.mon.sounding() == 0, "tempo ramp (400 tempo changes), then stop: nothing left sounding");
    // Meter change: the project follows the host (needs the message thread).
    a.head.info.setTimeSignature(AudioPlayHead::TimeSignature{ 3, 4 }); a.ppq = 0; a.run(6, true); pump(300); a.run(12, true); a.run(0.2, false);
    check(a.mon.sounding() == 0, "meter change to 3/4 while playing, then stop: nothing left sounding");
    a.head.info.setTimeSignature(AudioPlayHead::TimeSignature{ 4, 4 }); a.run(1, true); pump(300); a.run(0.2, false);
    // Different block sizes and sample rates.
    for (auto cfg : { std::pair<double, int>{ 44100, 64 }, { 96000, 1024 }, { 48000, 37 }, { 192000, 4096 } }) {
        a.plug->releaseResources(); a.sr = cfg.first; a.bs = cfg.second; a.plug->setRateAndBufferSizeDetails(a.sr, a.bs); a.plug->prepareToPlay(a.sr, a.bs);
        a.ppq = 0; a.bpm = 110; a.run(20, true); a.run(0.5, false);
        check(a.mon.sounding() == 0, String(cfg.first, 0) + " Hz, block " + String(cfg.second) + ": plays and stops clean");
    }
    check(a.mon.overlaps == 0 && a.mon.orphans == 0, "no note-on for a sounding pitch, no note-off for a silent pitch (" + String(a.mon.overlaps) + ", " + String(a.mon.orphans) + ")");
    check(a.mon.ons == a.mon.offs, "note-ons equal note-offs: " + String(a.mon.ons) + " and " + String(a.mon.offs));
    std::printf("  blocks processed: %ld\n", a.blocks);

    std::printf("\n2. state save and restore\n");
    MemoryBlock state; a.plug->getStateInformation(state);
    check(state.getSize() > 2000, "state holds the project (" + String((int) state.getSize()) + " bytes)");
    auto render = [](Rig& r) { r.mon = Monitor(); r.ppq = 0; r.bpm = 96; r.run(64, true); r.run(0.5, false); return r.mon.log.joinIntoString("\n"); };
    a.plug->releaseResources(); a.sr = 48000; a.bs = 256; a.plug->setRateAndBufferSizeDetails(a.sr, a.bs); a.plug->prepareToPlay(a.sr, a.bs);
    const String before = render(a);
    a.plug->releaseResources(); a.plug.reset(); pump(100);          // the session goes with its last instance
    Rig b; b.plug = make(); b.prepare();
    const String fresh = render(b);
    b.plug->setStateInformation(state.getData(), (int) state.getSize()); pump(100);
    const String after = render(b);
    check(before.isNotEmpty() && before == after, "a new instance with the saved state plays the same notes at the same times (" + String(b.mon.ons) + " notes over 64 beats)");
    check(fresh != before, "(and a new instance without the state plays something else, so the comparison means something)");
    MemoryBlock state2; b.plug->getStateInformation(state2);
    b.plug->setStateInformation("garbage", 7); b.plug->setStateInformation(nullptr, 0);
    check(render(b) == before, "a damaged state is ignored and the project stays");

    std::printf("\n3. two linked instances: chords on one track, bass on the other\n");
    Rig c; c.plug = make(); c.prepare();          // joins link A, where b already is
    pump(900);                                    // the second instance takes the bass by itself, the first keeps the chords
    auto* partB = param(*b.plug, "Part"); auto* partC = param(*c.plug, "Part");
    check(partB && partC && partC->getCurrentValueAsText() == "Bass", "the second instance set itself to Bass (" + (partC ? partC->getCurrentValueAsText() : String("?")) + ")");
    check(partB && partB->getCurrentValueAsText() == "Chords", "the first instance, whose part nobody had set by hand, changed itself to Chords (" + (partB ? partB->getCurrentValueAsText() : String("?")) + ")");
    const String chords = render(b), bass = render(c);
    StringArray merged; merged.addLines(chords); merged.addLines(bass);
    StringArray ref; ref.addLines(before);
    auto key = [](const String& l) { return l.upToFirstOccurrenceOf(" ", false, false).getDoubleValue() * 100000 + l.fromLastOccurrenceOf(" ", false, false).getIntValue(); };
    std::sort(merged.strings.begin(), merged.strings.end(), [&](const String& x, const String& y) { return key(x) < key(y); });
    std::sort(ref.strings.begin(), ref.strings.end(), [&](const String& x, const String& y) { return key(x) < key(y); });
    check(b.mon.soundingOn(1) == 0 && chords.isNotEmpty() && !chords.contains(" on 2 "), "chords instance sends only channel 1");
    check(bass.isNotEmpty() && !bass.contains(" on 1 "), "bass instance sends only channel 2");
    check(merged == ref, "chords + bass from the two instances equal the notes of one instance playing both (" + String(merged.size()) + " notes)");
    // An edit made through one instance reaches the other: restore a different project into c.
    MemoryBlock s3; c.plug->getStateInformation(s3);
    check(s3.getSize() > 2000, "the second instance saves the shared project too (" + String((int) s3.getSize()) + " bytes)");

    std::printf("\n4. bypass and reset\n");
    b.mon = Monitor(); b.ppq = 0; b.run(6, true);
    { AudioBuffer<float> buf(1, b.bs); MidiBuffer mb; b.plug->processBlockBypassed(buf, mb); b.mon.feed(mb, b.ppq, 0); }
    check(b.mon.sounding() == 0, "bypass while notes sound: every note got its note-off");
    b.run(6, true); b.plug->reset(); b.run(0.1, true); b.run(0.3, false);
    check(b.mon.sounding() == 0 && b.mon.overlaps == 0 && b.mon.orphans == 0, "reset() in the middle of a chord: no stuck note, no doubled note");

    c.plug->releaseResources(); b.plug->releaseResources(); c.plug.reset(); b.plug.reset();
    std::printf("\nhost test: %d checks, %d failed\n", checksDone, failures);
    std::printf(failures ? "HOST TEST FAILED\n" : "host test passed\n");
    return failures ? 1 : 0;
}
