#pragma once
#include "PluginProcessor.h"

struct PedalLNF : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        auto c = b.getCentre();
        float r = juce::jmin (b.getWidth(), b.getHeight()) / 2.0f;
        const bool big = s.getName() == "preset";
        g.setColour (big ? juce::Colour (0xfff4f4f0) : juce::Colour (0xff0e0e0e));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.5f);
        float ang = a0 + pos * (a1 - a0);
        g.setColour (big ? juce::Colours::black : juce::Colours::white);
        g.drawLine (c.x + r * 0.30f * std::sin (ang), c.y - r * 0.30f * std::cos (ang),
                    c.x + r * 0.85f * std::sin (ang), c.y - r * 0.85f * std::cos (ang), 2.5f);
    }
};

class RingEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RingEditor (RingProcessor&);
    ~RingEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    RingProcessor& proc;
    PedalLNF lnf;
    juce::Slider blend, wave, filter, fine, coarse, preset;
    juce::TextButton rm { "RM" }, ub { "UB" }, lb { "LB" }, bypass { "BYPASS" }, presetStep { "PRESET / TUNE" };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aBlend, aWave, aFilter, aFine, aCoarse, aPreset;
    std::unique_ptr<BA> aBypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingEditor)
};
