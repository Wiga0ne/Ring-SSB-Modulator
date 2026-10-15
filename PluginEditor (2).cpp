#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    // Design size = size of background.png. Every coordinate below is in these pixels.
    constexpr int kW = 993, kH = 822;
    const float kKnobX[5] = { 97.5f, 257.5f, 417.5f, 577.5f, 737.5f };
    constexpr float kKnobY = 181.0f, kKnobR = 60.0f;
    constexpr float kBigX = 892.0f, kBigY = 179.0f;
    constexpr float kLadderX = 657.0f, kLadderY0 = 309.0f, kLadderDY = 34.5f;
    const juce::Colour kOrange (0xffff9a1a), kRed (0xffff2a2a), kBtnOrange (0xfff95527), kYellow (0xffffd600);
    const float kScales[3] = { 0.5f, 0.75f, 1.0f };
    constexpr float kDefaultScale = 0.6f;

    void drawLed (juce::Graphics& g, float cx, float cy, float r, bool lit, juce::Colour c)
    {
        g.setColour (juce::Colour (0xff0a0603));
        g.fillEllipse (cx - r - 2.5f, cy - r - 2.5f, 2 * r + 5, 2 * r + 5);
        if (lit)
        {
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.38f), cx, cy, c.withAlpha (0.0f), cx + r * 2.4f, cy, true));
            g.fillEllipse (cx - r * 2.4f, cy - r * 2.4f, r * 4.8f, r * 4.8f);
            g.setGradientFill (juce::ColourGradient (c.brighter (0.9f), cx - r * 0.3f, cy - r * 0.3f, c.darker (0.15f), cx + r, cy + r, true));
        }
        else
            g.setColour (c.darker (0.88f));
        g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
    }

    void setParamGesture (juce::AudioProcessorValueTreeState& apvts, const char* id, float v, bool begin, bool end)
    {
        if (auto* p = apvts.getParameter (id))
        {
            if (begin) p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            if (end) p->endChangeGesture();
        }
    }
}

static void addSizeSubmenu (juce::PopupMenu&, juce::Component&);
static void handleSizeChoice (int, juce::Component*);
static void showSizeMenuFor (juce::Component&);

// knob that lets a right click open the size menu instead of dragging
struct PedalSlider : juce::Slider
{
    void mouseDown (const juce::MouseEvent& e) override
    {
        passThrough = e.mods.isPopupMenu();
        if (passThrough) showSizeMenuFor (*this); else juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! passThrough) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! passThrough) juce::Slider::mouseUp (e); passThrough = false; }
    bool passThrough = false;
};

// ------------------------------------------------------------------ knob look
struct PedalKnobLNF : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto c = b.getCentre();
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        g.setColour (juce::Colour (0xff060606));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        const float r2 = r * 0.95f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3f40), c.x - r * 0.35f, c.y - r * 0.5f,
                                                 juce::Colour (0xff1c1d1d), c.x + r * 0.6f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r2, c.y - r2, 2 * r2, 2 * r2);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawEllipse (c.x - r * 0.80f, c.y - r * 0.80f, r * 1.6f, r * 1.6f, 2.0f);
        const float ang = a0 + pos * (a1 - a0);
        const float sx = std::sin (ang), sy = -std::cos (ang);
        g.setColour (juce::Colours::white);
        g.drawLine (c.x + sx * r * 0.28f, c.y + sy * r * 0.28f, c.x + sx * r * 0.78f, c.y + sy * r * 0.78f, 7.0f);
        g.fillEllipse (c.x + sx * r * 0.28f - 3.5f, c.y + sy * r * 0.28f - 3.5f, 7.0f, 7.0f);
        g.fillEllipse (c.x + sx * r * 0.78f - 3.5f, c.y + sy * r * 0.78f - 3.5f, 7.0f, 7.0f);
    }
};

// ------------------------------------------------------------------ big MODE / preset knob
class BigKnob : public juce::Component, private juce::Timer
{
public:
    explicit BigKnob (RingProcessor& p) : proc (p) {}

    bool isHoldBlinking() const { return holding && ! moved && (juce::Time::getMillisecondCounter() - downTime) > 2000; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float r = 58.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2edd8), c.x - r * 0.6f, c.y - r * 0.8f,
                                                 juce::Colour (0xffbdb596), c.x + r * 0.7f, c.y + r * 0.9f, true));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colour (0xff111111));
        g.drawEllipse (c.x - 49.0f, c.y - 49.0f, 98.0f, 98.0f, 3.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffaf6e6), c.x - 20, c.y - 30, juce::Colour (0xffe6e0c8), c.x + 30, c.y + 40, true));
        g.fillEllipse (c.x - 46.0f, c.y - 46.0f, 92.0f, 92.0f);
        const int slot = proc.getCurrentSlot();
        const float ang = juce::degreesToRadians (-135.0f + 270.0f * (float) slot / 9.0f);
        g.setColour (juce::Colour (0xff5a5238));
        g.fillEllipse (c.x + std::sin (ang) * 34.0f - 4.0f, c.y - std::cos (ang) * 34.0f - 4.0f, 8.0f, 8.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        holding = true; moved = false; accum = 0.0f;
        lastX = e.x; lastY = e.y; downTime = juce::Time::getMillisecondCounter();
        startTimerHz (20);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! holding) return;
        if (e.getDistanceFromDragStart() > 3) moved = true;
        accum += (float) ((e.x - lastX) - (e.y - lastY));
        lastX = e.x; lastY = e.y;
        while (accum >= 14.0f)  { accum -= 14.0f; step (+1); }
        while (accum <= -14.0f) { accum += 14.0f; step (-1); }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! holding) return;
        holding = false; stopTimer();
        const auto dt = juce::Time::getMillisecondCounter() - downTime;
        if (! moved && dt < 500) proc.cycleModes();    // push = change mode
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        step (w.deltaY > 0 ? +1 : -1);
    }

private:
    void step (int d) { proc.loadPreset (juce::jlimit (0, 9, proc.getCurrentSlot() + d)); repaint(); }

    void timerCallback() override
    {
        if (! holding || moved) { stopTimer(); return; }
        if (juce::Time::getMillisecondCounter() - downTime >= 3000)
        {
            holding = false; stopTimer();
            if (proc.getCurrentSlot() > 0) proc.saveUserPreset (proc.getCurrentSlot());   // hold 3 s = save
        }
    }

    void showMenu()
    {
        const int slot = proc.getCurrentSlot();
        juce::PopupMenu m;
        m.addItem (1, slot > 0 ? "Save current sound to preset " + juce::String (slot) : "Save (turn the knob to a preset first)", slot > 0);
        m.addItem (2, slot > 0 ? "Restore factory preset " + juce::String (slot) : "Restore factory preset", slot > 0);
        m.addItem (3, "Restore ALL factory presets");
        m.addSeparator();
        m.addItem (4, "Unload preset (live knob positions)", slot > 0);
        m.addItem (5, "Next mode combination (same as pushing the knob)");
        m.addItem (6, "Load the factory Phaser sound (keeps your 9 presets untouched)");
        addSizeSubmenu (m, *this);
        juce::Component::SafePointer<BigKnob> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) {
            if (sp == nullptr) return;
            auto& p = sp->proc; const int s = p.getCurrentSlot();
            if (r == 1) p.saveUserPreset (s);
            else if (r == 2) p.restoreFactoryPreset (s);
            else if (r == 3) p.restoreAllFactory();
            else if (r == 4) p.loadPreset (0);
            else if (r == 5) p.cycleModes();
            else if (r == 6) p.loadFactorySound (9);
            else handleSizeChoice (r, sp.getComponent());
        });
    }

    RingProcessor& proc;
    bool holding = false, moved = false;
    float accum = 0.0f; int lastX = 0, lastY = 0;
    juce::uint32 downTime = 0;
};

// ------------------------------------------------------------------ invisible click zones
class ToggleZone : public juce::Component
{
public:
    ToggleZone (RingProcessor& p, const char* paramId) : proc (p), id (paramId) {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showSizeMenuFor (*this); return; }
        const float cur = proc.apvts.getRawParameterValue (id)->load();
        setParamGesture (proc.apvts, id, cur > 0.5f ? 0.0f : 1.0f, true, true);
    }
    void paint (juce::Graphics&) override {}
private:
    RingProcessor& proc; const char* id;
};

class LadderZone : public juce::Component
{
public:
    explicit LadderZone (RingProcessor& p) : proc (p) {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showSizeMenuFor (*this); return; }
        const int row = (int) ((e.y) / kLadderDY);
        proc.loadPreset (juce::jlimit (1, 9, row + 1));
    }
    void paint (juce::Graphics&) override {}
private:
    RingProcessor& proc;
};

// ------------------------------------------------------------------ footswitches
class Footswitch : public juce::Component
{
public:
    Footswitch (RingProcessor& p, bool presetSwitch) : proc (p), isPreset (presetSwitch) {}
    ~Footswitch() override
    {
        if (down && isPreset) setParamGesture (proc.apvts, "presetfsw", 0.0f, false, true);   // never leave the switch stuck
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (3.0f);
        const float w = b.getWidth(), h = b.getHeight();
        juce::Path hex;
        hex.startNewSubPath (b.getX(), b.getCentreY());
        hex.lineTo (b.getX() + w * 0.25f, b.getY());
        hex.lineTo (b.getX() + w * 0.75f, b.getY());
        hex.lineTo (b.getRight(), b.getCentreY());
        hex.lineTo (b.getX() + w * 0.75f, b.getBottom());
        hex.lineTo (b.getX() + w * 0.25f, b.getBottom());
        hex.closeSubPath();
        auto rounded = hex.createPathWithRoundedCorners (7.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e9e9), b.getX(), b.getY(), juce::Colour (0xff8a8a8a), b.getX(), b.getBottom(), false));
        g.fillPath (rounded);
        g.setColour (juce::Colour (0xff111111));
        g.strokePath (rounded, juce::PathStrokeType (3.0f));
        const auto c = b.getCentre();
        const float r = 37.0f;
        g.setColour (juce::Colour (0xff3a3a3a));
        g.fillEllipse (c.x - r - 4, c.y - r - 4, 2 * r + 8, 2 * r + 8);
        const float off = down ? 1.5f : 0.0f;
        g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xffaaaaaa) : juce::Colour (0xffe4e4e4), c.x - 12, c.y - 18 + off,
                                                 down ? juce::Colour (0xff7d7d7d) : juce::Colour (0xffa9a9a9), c.x + 18, c.y + 26 + off, true));
        g.fillEllipse (c.x - r, c.y - r + off, 2 * r, 2 * r);
        if (proc.getLearnTarget() == target() && (juce::Time::getMillisecondCounter() / 300) % 2 == 0)
        {
            g.setColour (kOrange);
            g.drawEllipse (c.x - r - 7, c.y - r - 7, 2 * r + 14, 2 * r + 14, 3.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        down = true;
        if (isPreset) setParamGesture (proc.apvts, "presetfsw", 1.0f, true, false);
        else
        {
            const float cur = proc.apvts.getRawParameterValue ("active")->load();
            setParamGesture (proc.apvts, "active", cur > 0.5f ? 0.0f : 1.0f, true, true);
        }
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! down) return;
        down = false;
        if (isPreset) setParamGesture (proc.apvts, "presetfsw", 0.0f, false, true);
        repaint();
    }

private:
    MidiTarget target() const { return isPreset ? MidiTarget::PresetFsw : MidiTarget::Bypass; }

    void showMenu()
    {
        const auto desc = proc.getMidiDescription (target());
        juce::PopupMenu m;
        m.addItem (1, "MIDI learn  (then move a pedal / press a note)");
        m.addItem (2, "Clear MIDI assignment", desc.isNotEmpty());
        m.addSeparator();
        m.addItem (3, desc.isNotEmpty() ? "Assigned to: " + desc : "No MIDI assignment", false);
        if (proc.getLearnTarget() == target()) m.addItem (5, "Cancel MIDI learn");
        if (! isPreset) m.addItem (6, "Latching MIDI switch (toggle on press AND release)", true, proc.isBypassLatching());
        m.addItem (7, "MIDI Program Change 1-9 also selects presets (0 = live knobs)", false);
        addSizeSubmenu (m, *this);
        m.addItem (4, "Tip: in your DAW you can also map the automatable parameter \""
                          + juce::String (isPreset ? "Preset/Tune Footswitch" : "Effect On (Bypass switch)") + "\"", false);
        juce::Component::SafePointer<Footswitch> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) {
            if (sp == nullptr) return;
            if (r == 1) sp->proc.startMidiLearn (sp->target());
            if (r == 2) sp->proc.clearMidi (sp->target());
            if (r == 5) sp->proc.cancelMidiLearn();
            if (r == 6) sp->proc.setBypassLatching (! sp->proc.isBypassLatching());
            handleSizeChoice (r, sp.getComponent());
        });
    }

    RingProcessor& proc;
    bool isPreset, down = false;
};

// ------------------------------------------------------------------ whole pedal
class PedalContent : public juce::Component, private juce::Timer
{
public:
    explicit PedalContent (RingProcessor& p)
        : proc (p), bigKnob (p), rmZone (p, "rm"), ubZone (p, "ub"), lbZone (p, "lb"), psZone (p, "ps"), phZone (p, "ph"),
          ladder (p), presetSw (p, true), bypassSw (p, false)
    {
        bg = juce::ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
        psWhite  = juce::ImageCache::getFromMemory (BinaryData::pitchshift_white_png,  BinaryData::pitchshift_white_pngSize);
        psYellow = juce::ImageCache::getFromMemory (BinaryData::pitchshift_yellow_png, BinaryData::pitchshift_yellow_pngSize);
        setSize (kW, kH);

        juce::Slider* ks[5] = { &blend, &wave, &filter, &fine, &coarse };
        const char* ids[5] = { "blend", "wave", "filter", "fine", "coarse" };
        for (int i = 0; i < 5; ++i)
        {
            auto& s = *ks[i];
            s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            s.setMouseDragSensitivity (240);
            s.setLookAndFeel (&lnf);
            addAndMakeVisible (s);
            s.setBounds ((int) (kKnobX[i] - kKnobR), (int) (kKnobY - kKnobR), (int) (2 * kKnobR), (int) (2 * kKnobR));
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], s));
            s.setDoubleClickReturnValue (true, proc.apvts.getParameter (ids[i])->convertFrom0to1 (proc.apvts.getParameter (ids[i])->getDefaultValue()));
        }

        addAndMakeVisible (bigKnob);  bigKnob.setBounds ((int) kBigX - 60, (int) kBigY - 60, 120, 120);
        addAndMakeVisible (rmZone);   rmZone.setBounds (829, 253, 39, 85);
        addAndMakeVisible (ubZone);   ubZone.setBounds (873, 253, 38, 85);
        addAndMakeVisible (lbZone);   lbZone.setBounds (914, 253, 40, 85);
        addAndMakeVisible (psZone);   psZone.setBounds (830, 354, 126, 24);
        addAndMakeVisible (phZone);   phZone.setBounds (690, 28, 104, 24);
        addAndMakeVisible (ladder);   ladder.setBounds (622, (int) (kLadderY0 - kLadderDY / 2), 194, (int) (kLadderDY * 9));
        addAndMakeVisible (presetSw); presetSw.setBounds (118, 638, 124, 110);
        addAndMakeVisible (bypassSw); bypassSw.setBounds (753, 638, 124, 110);
        startTimerHz (30);
    }

    ~PedalContent() override
    {
        for (auto* s : { &blend, &wave, &filter, &fine, &coarse }) s->setLookAndFeel (nullptr);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) showSizeMenuFor (*this);
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (bg, getLocalBounds().toFloat());

        const auto now = juce::Time::getMillisecondCounter();
        const int modes = proc.getModes();
        const int slot = proc.getCurrentSlot();
        const bool dirty = proc.isPresetDirty();
        const bool saveFlash = (now - proc.lastSaveMs) < 900 && ((now / 60) % 2 == 0);
        const bool holdBlink = bigKnob.isHoldBlinking() && ((now / 60) % 2 == 0);

        // mode buttons: orange fill + label when active, LEDs follow
        struct Btn { int bit; juce::Rectangle<int> r; const char* txt; float lx, ly, ledX, ledY; };
        const Btn btns[3] = {
            { kRM, { 829, 253, 39, 85 }, "RM", 850.f, 314.f, 850.f, 281.f },
            { kUB, { 873, 253, 38, 85 }, "UB", 893.f, 284.f, 893.f, 306.f },
            { kLB, { 914, 253, 40, 85 }, "LB", 934.f, 314.f, 934.f, 281.f } };
        for (const auto& b : btns)
        {
            const bool on = (modes & b.bit) != 0;
            if (on)
            {
                g.setColour (kBtnOrange);
                g.fillRect (b.r);
                g.setColour (juce::Colours::white);
                g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
                g.drawText (b.txt, juce::Rectangle<float> (b.lx - 20.0f, b.ly - 10.0f, 40.0f, 20.0f), juce::Justification::centred);
            }
            drawLed (g, b.ledX, b.ledY, 10.0f, on, kOrange);
        }
        // PITCH-SHIFT label: yellow = on, white = off (the label lives in two sprites, not in the background)
        g.drawImageAt ((modes & kPS) ? psYellow : psWhite, 829, 356);

        // PHASER switch (top right of the body): LED + label, yellow when on
        const bool phOn = (modes & kPH) != 0;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (690.0f, 28.0f, 104.0f, 24.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (690.5f, 28.5f, 103.0f, 23.0f, 6.0f, 1.0f);
        drawLed (g, 704.0f, 40.0f, 5.5f, phOn, kOrange);
        g.setColour (phOn ? kYellow : juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText ("PHASER", juce::Rectangle<float> (714.0f, 28.0f, 76.0f, 24.0f), juce::Justification::centred);

        // preset ladder LEDs
        for (int i = 0; i < 9; ++i)
            drawLed (g, kLadderX, kLadderY0 + kLadderDY * (float) i, 11.0f, (i + 1 == slot) || saveFlash || holdBlink, kOrange);

        // PRESET LED: solid = preset loaded, fast blink = knobs moved since loading, slow blink = tuning
        bool presetLit = slot > 0;
        if (slot > 0 && dirty) presetLit = (now / 120) % 2 == 0;
        if (proc.isTuning()) presetLit = (now / 250) % 2 == 0;
        drawLed (g, 362.0f, 674.0f, 12.0f, presetLit, kOrange);
        drawLed (g, 633.0f, 674.0f, 12.0f, proc.apvts.getRawParameterValue ("active")->load() > 0.5f, kRed);

        // small status line
        juce::String status;
        if (proc.isTunedActive()) status << "TUNED " << juce::String (proc.getTunedHz(), 1) << " Hz   ";
        const auto m1 = proc.getMidiDescription (MidiTarget::PresetFsw), m2 = proc.getMidiDescription (MidiTarget::Bypass);
        if (m1.isNotEmpty()) status << "MIDI PRESET/TUNE: " << m1 << "   ";
        if (m2.isNotEmpty()) status << "MIDI BYPASS: " << m2;
        if (proc.getLearnTarget() != MidiTarget::None) status = "MIDI LEARN: move a control / press a note...";
        if (status.isNotEmpty())
        {
            g.setColour (juce::Colour (0xffc8c8c8));
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            g.drawText (status, 200, 784, 600, 18, juce::Justification::centred);
        }
    }

private:
    void timerCallback() override { repaint(); }

    RingProcessor& proc;
    juce::Image bg;
    PedalKnobLNF lnf;
    PedalSlider blend, wave, filter, fine, coarse;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::Image psWhite, psYellow;
    BigKnob bigKnob;
    ToggleZone rmZone, ubZone, lbZone, psZone, phZone;
    LadderZone ladder;
    Footswitch presetSw, bypassSw;
};

// ------------------------------------------------------------------ editor
RingEditor::RingEditor (RingProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<PedalContent> (proc);
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (kW / 2, kH / 2, 1500, 1242);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setScaleIndex (juce::jlimit (-1, 2, proc.uiScaleIdx.load()));   // default: 60 %
}

RingEditor::~RingEditor() = default;

void RingEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (juce::AffineTransform::scale (s));
    content->setBounds (0, 0, kW, kH);
}

int RingEditor::getScaleIndex() const
{
    const float s = (float) getWidth() / (float) kW;
    for (int i = 0; i < 3; ++i) if (std::abs (s - kScales[i]) < 0.02f) return i;
    return -1;
}

void RingEditor::setScaleIndex (int idx)
{
    idx = juce::jlimit (-1, 2, idx);
    proc.uiScaleIdx = idx;
    const float s = idx < 0 ? kDefaultScale : kScales[idx];
    setSize ((int) std::lround (kW * s), (int) std::lround (kH * s));
}

static void addSizeSubmenu (juce::PopupMenu& m, juce::Component& c)
{
    const auto* ed = c.findParentComponentOfClass<RingEditor>();
    const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
    const char* names[3] = { "50 %", "75 %", "100 %" };
    juce::PopupMenu sub;
    for (int i = 0; i < 3; ++i) sub.addItem (101 + i, names[i], true, i == cur);
    m.addSeparator();
    m.addSubMenu ("Plugin size", sub);
}

static void handleSizeChoice (int r, juce::Component* c)
{
    if (c == nullptr || r < 101 || r > 103) return;
    if (auto* ed = c->findParentComponentOfClass<RingEditor>()) ed->setScaleIndex (r - 101);
}

static void showSizeMenuFor (juce::Component& c)
{
    const auto* ed = c.findParentComponentOfClass<RingEditor>();
    const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
    const char* names[3] = { "50 %", "75 %", "100 %" };
    juce::PopupMenu m;
    m.addSectionHeader ("Plugin size");
    for (int i = 0; i < 3; ++i) m.addItem (101 + i, names[i], true, i == cur);
    juce::Component::SafePointer<juce::Component> sp (&c);
    m.showMenuAsync (juce::PopupMenu::Options(), [sp] (int r) { handleSizeChoice (r, sp.getComponent()); });
}
