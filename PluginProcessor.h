#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

enum ModeBits { kRM = 1, kUB = 2, kLB = 4, kPS = 8, kPH = 16 };
enum class MidiTarget { None = 0, PresetFsw = 1, Bypass = 2 };

struct PresetData
{
    float blend = 0.5f, wave = 0.25f, filter = 1.0f, fine = 0.5f, coarse = 0.5f;
    int modes = kRM;
    float tunedHz = 0.0f;
};

class RingProcessor : public juce::AudioProcessor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater
{
public:
    RingProcessor();
    ~RingProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Wiga Ring Think"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Host-visible factory programs (the preset menu of the DAW)
    int getNumPrograms() override { return 10; }      // 9 preset slots + the factory Phaser sound
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return factoryNames[juce::jlimit (0, 9, index)]; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- presets ----
    static const char* const factoryNames[10];
    static PresetData factoryPreset (int index0to9);   // 0-8 = preset slots 1-9, 9 = Phaser
    int  getCurrentSlot() const { return currentSlot; }
    void loadPreset (int slot0to9);          // 0 = unload (live knobs)
    void loadFactorySound (int index0to9);   // applies a factory sound without touching the 9 user slots
    void saveUserPreset (int slot1to9);
    void restoreFactoryPreset (int slot1to9);
    void restoreAllFactory();
    bool isPresetDirty() const;
    void requestNextPreset();
    void cycleModes();
    int  getModes() const;
    juce::uint32 lastSaveMs = 0;

    // ---- tune / midi ----
    bool  isTuning() const { return tuning.load(); }
    bool  isTunedActive() const { return tunedActive.load(); }
    float getTunedHz() const { return tunedHz.load(); }
    void startMidiLearn (MidiTarget t) { learnTarget = (int) t; }
    void cancelMidiLearn() { learnTarget = 0; }
    bool isBypassLatching() const { return bypassLatching.load(); }
    void setBypassLatching (bool b) { bypassLatching = b; }
    std::atomic<int> uiScaleIdx { -1 };       // -1 = default size (60 %), 0..2 = 50 / 75 / 100 %
    MidiTarget getLearnTarget() const { return (MidiTarget) learnTarget.load(); }
    void clearMidi (MidiTarget t);
    juce::String getMidiDescription (MidiTarget t) const;

    void setParamValue (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState apvts;

private:
    struct AP {
        float a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float process (float x) { float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
    };
    struct Chan {
        AP p1[4], p2[4];
        float z1 = 0, l1 = 0, l2 = 0, l3 = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;
    bool handleMidi (juce::MidiBuffer&);   // returns true if a PRESET/TUNE press arrived in this block
    float detectPitch();
    void applyPresetData (const PresetData&);
    PresetData captureCurrent() const;
    static juce::File getPresetFile();
    void loadUserPresetsFromDisk();
    void writeUserPresetsToDisk();

    Chan chans[2];
    double sr = 44100.0;
    float carrierPhase = 0.f, lfoPhase = 0.f, psPhase = 0.f;
    std::vector<float> psBuf[2];
    int psMask = 0, psPos = 0, psWin = 1024;

    juce::SmoothedValue<float> freqSm, blendSm, cutSm, psBaseSm, depthSm, activeSm;

    static constexpr int tuneSize = 4096;
    std::vector<float> tuneBuf, tuneWork, yinD;
    int tuneWrite = 0, tuneCounter = 0;

    std::atomic<float> *pBlend, *pWave, *pFilter, *pFine, *pCoarse, *pRM, *pUB, *pLB, *pPS, *pPH, *pActive, *pFsw, *pPreset;

    bool prevDown = false;
    double holdSamples = 0.0;
    std::atomic<bool> tuning { false }, tunedActive { false };
    std::atomic<float> tunedHz { 0.f };
    float tunedCoarse = 0.f, tunedFine = 0.f;

    std::atomic<bool> midiPresetDown { false }, midiBypassHeld { false }, bypassLatching { false };
    float phS[2][6] = {};
    float phFb[2] = { 0.f, 0.f };
    float phLfoPhase = 0.f;
    std::atomic<int> learnTarget { 0 }, ccPreset { -1 }, notePreset { -1 }, ccBypass { -1 }, noteBypass { -1 };

    std::atomic<bool> pendingNext { false }, pendingToggle { false };
    std::atomic<int> pendingPreset { -1 }, pendingProgram { -1 };
    int currentSlot = 0, currentProgram = 0;
    juce::uint32 programLockStart = 0;
    PresetData userPresets[9];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingProcessor)
};
