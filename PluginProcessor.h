#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

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
    const juce::String getName() const override { return "Ring SSB Modulator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    struct AP {
        float a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float process (float x) {
            float y = a2 * (x + y2) - x2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };
    struct Chan {
        AP p1[4], p2[4];
        float z1 = 0, lp1 = 0, lp2 = 0;
    };

    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    Chan chans[2];
    double sr = 44100.0;
    double phase = 0.0;
    juce::SmoothedValue<float> freqSm, blendSm, cutSm;
    std::atomic<int> pendingPreset { 0 };
    std::atomic<bool> loading { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingProcessor)
};
