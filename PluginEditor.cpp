#include "PluginEditor.h"

RingEditor::RingEditor (RingProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (700, 630);

    auto setup = [this] (juce::Slider& s, const juce::String& name) {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        addAndMakeVisible (s);
    };
    setup (blend, "blend"); setup (wave, "wave"); setup (filter, "filter");
    setup (fine, "fine");   setup (coarse, "coarse"); setup (preset, "preset");
    preset.setNumDecimalPlacesToDisplay (0);

    aBlend  = std::make_unique<SA> (proc.apvts, "blend", blend);
    aWave   = std::make_unique<SA> (proc.apvts, "wave", wave);
    aFilter = std::make_unique<SA> (proc.apvts, "filter", filter);
    aFine   = std::make_unique<SA> (proc.apvts, "fine", fine);
    aCoarse = std::make_unique<SA> (proc.apvts, "coarse", coarse);
    aPreset = std::make_unique<SA> (proc.apvts, "preset", preset);

    int idx = 0;
    for (auto* b : { &rm, &ub, &lb }) {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff222222));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8412f));
        b->onClick = [this, idx] {
            if (auto* prm = proc.apvts.getParameter ("mode"))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
        };
        addAndMakeVisible (*b);
        ++idx;
    }

    bypass.setClickingTogglesState (true);
    bypass.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb8b8b8));
    bypass.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff777777));
    bypass.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    bypass.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    aBypass = std::make_unique<BA> (proc.apvts, "bypass", bypass);
    addAndMakeVisible (bypass);

    presetStep.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb8b8b8));
    presetStep.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    presetStep.onClick = [this] {
        int v = (int) proc.apvts.getRawParameterValue ("preset")->load();
        v = v % 9 + 1;
        if (auto* prm = proc.apvts.getParameter ("preset"))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
    };
    addAndMakeVisible (presetStep);

    startTimerHz (15);
    timerCallback();
}

void RingEditor::resized()
{
    const int cx[5] = { 75, 185, 295, 405, 515 };
    juce::Slider* k[5] = { &blend, &wave, &filter, &fine, &coarse };
    for (int i = 0; i < 5; ++i) k[i]->setBounds (cx[i] - 35, 150, 70, 70);
    preset.setBounds (627 - 42, 185 - 42, 84, 84);
    rm.setBounds (578, 255, 36, 28); ub.setBounds (620, 255, 36, 28); lb.setBounds (662, 255, 36, 28);
    presetStep.setBounds (90, 515, 80, 70);
    bypass.setBounds (530, 515, 80, 70);
}

void RingEditor::timerCallback()
{
    const int m = (int) proc.apvts.getRawParameterValue ("mode")->load();
    rm.setToggleState (m == 0, juce::dontSendNotification);
    ub.setToggleState (m == 1, juce::dontSendNotification);
    lb.setToggleState (m == 2, juce::dontSendNotification);
    repaint();
}

void RingEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xfff2efe6));
    auto body = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (juce::Colour (0xff1b1b1b));
    g.fillRoundedRectangle (body, 18.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    const char* names[5] = { "BLEND", "WAVE", "FILTER/RATE", "FINE/DEPTH", "COARSE" };
    const int cx[5] = { 75, 185, 295, 405, 515 };
    for (int i = 0; i < 5; ++i)
        g.drawText (names[i], cx[i] - 55, 232, 110, 18, juce::Justification::centred);
    g.drawText ("9V", 310, 18, 40, 16, juce::Justification::centred);

    // wave icons around the WAVE knob
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("sin", 125, 120, 30, 14, juce::Justification::centred);
    g.drawText ("sqr", 215, 120, 30, 14, juce::Justification::centred);

    // mode panel
    g.setColour (juce::Colour (0xfff4f4f0));
    g.drawRoundedRectangle (565.0f, 100.0f, 125.0f, 245.0f, 8.0f, 2.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("MODE", 565, 104, 125, 16, juce::Justification::centred);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("TURN PRESETS", 565, 118, 125, 12, juce::Justification::centred);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("PITCH-SHIFT", 565, 300, 125, 16, juce::Justification::centred);

    // title
    g.setColour (juce::Colour (0xffe2432f));
    g.setFont (juce::FontOptions (112.0f, juce::Font::bold));
    g.drawText ("RING", 55, 262, 380, 100, juce::Justification::centredLeft);
    g.drawText ("THING", 55, 360, 380, 100, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("SINGLE SIDEBAND MODULATOR", 62, 468, 380, 20, juce::Justification::centredLeft);

    // preset list
    const int cur = (int) proc.apvts.getRawParameterValue ("preset")->load();
    g.setColour (juce::Colour (0xfff4f4f0));
    g.drawRoundedRectangle (435.0f, 275.0f, 130.0f, 215.0f, 6.0f, 2.0f);
    for (int i = 1; i <= 9; ++i) {
        const float y = 278.0f + (float) (i - 1) * 23.5f;
        g.setColour (i == cur ? juce::Colour (0xffffa000) : juce::Colour (0xff6a4a10));
        g.fillEllipse (445.0f, y + 5.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("PRESET " + juce::String (i), 462, (int) y + 3, 100, 16, juce::Justification::centredLeft);
    }

    // LEDs and footswitch labels
    g.setColour (juce::Colour (0xffffa000));
    g.fillEllipse (252.0f, 540.0f, 18.0f, 18.0f);
    g.setColour (proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f ? juce::Colour (0xff4a1010) : juce::Colour (0xffe01818));
    g.fillEllipse (420.0f, 540.0f, 18.0f, 18.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("PRESET", 190, 518, 60, 16, juce::Justification::centred);
    g.drawText ("STATUS", 440, 518, 60, 16, juce::Justification::centred);
    g.drawText ("PRESET / TUNE", 70, 592, 120, 16, juce::Justification::centred);
    g.drawText ("BYPASS", 520, 592, 100, 16, juce::Justification::centred);
    g.setFont (juce::FontOptions (22.0f, juce::Font::italic));
    g.drawText ("ring ssb modulator", 215, 570, 270, 28, juce::Justification::centred);
}
