#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Arranger.h"
#include "SynthVoices.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

class SoulChordProcessor : public juce::AudioProcessor,
                           private juce::Timer
{
public:
    enum PlayMode { ModeSequencer = 0, ModeTrigger = 1 };

    SoulChordProcessor();
    ~SoulChordProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SoulChord"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    std::shared_ptr<const sc::Performance> getPerformance() const { return std::atomic_load (&perf); }
    juce::String getCustomProgression() const;
    void setCustomProgression (const juce::String& text);

    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const;
    void applyGenreDefaults (int genre);

    // 0 = chords, 1 = bass, 2 = both
    juce::File writeMidiFile (int which) const;

    // GUI feedback (written by the audio thread)
    std::atomic<int> currentStep { -1 };
    std::atomic<int> heldRelMask { 0 };
    std::atomic<uint64_t> soundingChord[2] {}, soundingBass[2] {};
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> transportRunning { false };

    static juce::String vi (const char* utf8) { return juce::String::fromUTF8 (utf8); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void rebuildIfNeeded (bool force);

    // note output helpers (audio thread)
    void sendOn (int ch, int note, int vel, int samplePos);
    void sendOff (int ch, int note, int samplePos);
    void releaseAllSeq (int samplePos);
    void releaseAllTrigger (int samplePos);
    void triggerOn (const sc::Performance& p, int inNote, int vel, int samplePos);
    void startSeqNote (const sc::NoteEvent& e, double endAbs, int samplePos);
    void dispatchPending (int numSamples);
    void triggerOff (int inNote, int samplePos);
    void setSounding (int ch, int note, bool on);

    juce::Synthesiser chordSynth, bassSynth;
    std::atomic<int> soundSel { 0 };
    juce::AudioBuffer<float> bassBuffer;
    juce::MidiBuffer chordMidi, bassMidi, outMidi;

    std::shared_ptr<const sc::Performance> perf;
    std::vector<std::shared_ptr<const sc::Performance>> graveyard;
    juce::String lastSignature;
    std::atomic<bool> forceRebuild { false };
    std::vector<sc::Step> lastValidSteps;

    // sequencer state
    struct Active { int note, ch; double endAbs; };
    std::array<Active, 512> active {};
    int numActive = 0;
    bool seqRunning = false;
    double expectedPpq = 0.0;
    double previewPpq = 0.0;
    bool lastPreview = false;
    int lastMode = -1;

    // trigger state
    struct Held { int inNote = -1; int rel = 0; int n = 0; int notes[10] {}; int bass = -1; uint32_t started = 0; };
    std::array<Held, 16> held {};
    struct Pending { int64_t time; int note, vel, heldIdx, j; };
    std::array<Pending, 256> pending {};
    int numPending = 0;
    sc::Voicing prevTrig;
    bool havePrevTrig = false;
    int prevTrigBass = -1;

    int noteRef[2][128] {};
    int64_t sampleCounter = 0;
    double sr = 44100.0;
    float lastChordGain = 0.7f, lastBassGain = 0.7f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoulChordProcessor)
};
