#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Product.h"
#include "PadSynth.h"
#include "Player.h"
#include "Session.h"

namespace kindred {

// Everything the render thread needs, made on another thread and swapped in whole.
struct RenderData {
    Timeline arrangement, section;
    PlayState play;
    PlayParams params;
    double tempo = 60;       // project tempo, used when there is no host tempo
    int bpb = 4;
    int sectionIndex = -1;
};

class KindredProcessor : public juce::AudioProcessor, public Session::Listener, private juce::Timer,
                         private juce::AudioProcessorValueTreeState::Listener {
public:
    KindredProcessor();
    ~KindredProcessor() override;

    // --- juce::AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return KINDRED_PRODUCT_NAME; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return !standalone; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- for the editor (message thread)
    Session& session() { return *sess; }
    std::shared_ptr<Session> sessionPtr() { return sess; }
    juce::String link() const { return linkId; }
    void setLink(const juce::String& link);                 // "" = not linked, "A".."H"
    int linkedCount() const { return sess->memberCount(); }
    std::shared_ptr<const RenderData> renderData() const;
    void togglePreview();
    void stopPreview();
    void panic() { panicFlag.store(true); }
    void audition(const std::vector<int>& chordNotes, int bassNote);
    // A Standard MIDI File of a section (or the arrangement when sectionId is empty). parts: 0 both, 1 chords, 2 bass.
    std::vector<uint8_t> midiFor(const std::string& sectionId, int parts) const;
    juce::File writeDragFile(const std::string& sectionId, int parts) const;
    int partChoice() const { return (int) pPart->load(); }
    void setPart(int part, bool fromUser);
    bool isStandalone() const { return standalone; }
    static juce::File userFolder();
    static juce::File stylesFolder();
    static juce::PropertiesFile& settings();
    juce::StringArray styleProblems;

    juce::AudioProcessorValueTreeState apvts;
    struct Ui {
        std::atomic<bool> playing{ false }, hostPlaying{ false }, sectionScope{ false }, hostTempoKnown{ false };
        std::atomic<double> pos{ 0 }, bpm{ 60 }, hostPpq{ 0 };
        std::atomic<int> tsNum{ 4 }, tsDen{ 4 }, sounding{ 0 };
        std::atomic<uint64_t> activeLo{ 0 }, activeHi{ 0 };
        std::atomic<uint32_t> blocks{ 0 };
    } ui;
    std::atomic<bool> padOn{ true };
    std::atomic<float> padVolume{ 0.8f };
    int editorW = 0, editorH = 0;

    void sessionChanged() override;
private:
    void timerCallback() override;
    void parameterChanged(const juce::String&, float) override;
    void rebuild();
    PlayParams currentParams() const;
    void joinLink(const juce::String& link, const Json* state);
    template <class Emit> void endAudition(int sample, Emit&& emit);

    const bool standalone;
    std::shared_ptr<Session> sess;
    juce::String linkId;
    bool stateRestored = false, partTouched = false, settingPartInternally = false;
    int ticks = 0;

    // render data exchange
    juce::SpinLock exLock;
    std::shared_ptr<const RenderData> incoming, current;     // `current` belongs to the render thread
    bool fresh = false;
    std::shared_ptr<const RenderData> latest;                 // for the UI
    mutable std::mutex latestMx; std::mutex buildMx;
    std::atomic<bool> paramsDirty{ false }, panicFlag{ false }, previewEnded{ false };

    // render thread
    Player player;
    PadSynth pad;
    double sr = 44100;
    uint64_t lastPreviewGen = 0; double previewPos = 0; bool wasActive = false;
    struct AudNote { uint8_t ch, pitch; };
    AudNote audNotes[8]; int audCount = 0; long audRemaining = 0;
    juce::SpinLock audLock; int audPending[8]; int audPendingCount = 0; int audPendingBass = -1; std::atomic<int> audSerial{ 0 }; int audSeen = 0;

    std::atomic<float>* pPart; std::atomic<float>* pChordVel; std::atomic<float>* pBassVel; std::atomic<float>* pChordOct; std::atomic<float>* pBassOct;
    std::atomic<float>* pHumanize; std::atomic<float>* pStrum; std::atomic<float>* pGate; std::atomic<float>* pChordCh; std::atomic<float>* pBassCh; std::atomic<float>* pThru;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KindredProcessor)
};

}  // namespace kindred
