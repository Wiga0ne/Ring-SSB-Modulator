#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset { float coarse, fine, wave, filter, blend; int mode; };
    const Preset presets[9] = {
        { 0.20f,  0.00f, 0.0f, 1.0f, 1.0f, 0 },
        { 0.35f,  0.00f, 0.0f, 1.0f, 1.0f, 0 },
        { 0.50f,  0.00f, 0.5f, 0.9f, 1.0f, 0 },
        { 0.60f,  0.00f, 1.0f, 0.8f, 0.8f, 0 },
        { 0.25f,  0.10f, 0.0f, 1.0f, 0.7f, 1 },
        { 0.15f,  0.00f, 0.0f, 1.0f, 1.0f, 1 },
        { 0.30f, -0.10f, 0.0f, 1.0f, 0.7f, 2 },
        { 0.10f,  0.00f, 0.0f, 1.0f, 1.0f, 2 },
        { 0.45f,  0.00f, 0.3f, 0.7f, 1.0f, 0 } };

    // Hilbert / quadrature allpass network (Olli Niemitalo), coefficients a
    const double hA1[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    const double hA2[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
}

RingProcessor::RingProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    apvts.addParameterListener ("preset", this);
}

RingProcessor::~RingProcessor() { cancelPendingUpdate(); apvts.removeParameterListener ("preset", this); }

juce::AudioProcessorValueTreeState::ParameterLayout RingProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "blend", 1 },  "Blend",       0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "wave", 1 },   "Wave",        0.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "filter", 1 }, "Filter/Rate", 0.0f, 1.0f, 1.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "fine", 1 },   "Fine/Depth",  -1.0f, 1.0f, 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "coarse", 1 }, "Coarse",      0.0f, 1.0f, 0.35f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 },  "Mode", StringArray { "RM", "UB", "LB" }, 0));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "preset", 1 },   "Preset", 1, 9, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return { p.begin(), p.end() };
}

void RingProcessor::parameterChanged (const juce::String&, float v)
{
    if (loading.load()) return;
    pendingPreset = (int) v;
    triggerAsyncUpdate();
}

void RingProcessor::handleAsyncUpdate()
{
    int i = pendingPreset.load() - 1;
    if (i < 0 || i > 8) return;
    auto set = [this] (const char* id, float v) {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    const auto& pr = presets[i];
    set ("coarse", pr.coarse); set ("fine", pr.fine); set ("wave", pr.wave);
    set ("filter", pr.filter); set ("blend", pr.blend); set ("mode", (float) pr.mode);
}

bool RingProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
         || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void RingProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& c : chans) {
        c = Chan();
        for (int k = 0; k < 4; ++k) {
            c.p1[k].a2 = (float) (hA1[k] * hA1[k]);
            c.p2[k].a2 = (float) (hA2[k] * hA2[k]);
        }
    }
    freqSm.reset (sr, 0.02);  freqSm.setCurrentAndSetTargetValue (500.0f);
    blendSm.reset (sr, 0.02); blendSm.setCurrentAndSetTargetValue (1.0f);
    cutSm.reset (sr, 0.02);   cutSm.setCurrentAndSetTargetValue (20000.0f);
}

void RingProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f) return;

    const float blend  = apvts.getRawParameterValue ("blend")->load();
    const float wave   = apvts.getRawParameterValue ("wave")->load();
    const float filter = apvts.getRawParameterValue ("filter")->load();
    const float fine   = apvts.getRawParameterValue ("fine")->load();
    const float coarse = apvts.getRawParameterValue ("coarse")->load();
    const int   mode   = (int) apvts.getRawParameterValue ("mode")->load();

    // Coarse: 20 Hz -> 5 kHz (exponential); Fine: +/- 50 Hz around it
    freqSm.setTargetValue (juce::jmax (0.5f, 20.0f * std::pow (250.0f, coarse) + fine * 50.0f));
    blendSm.setTargetValue (blend);
    cutSm.setTargetValue (200.0f * std::pow (100.0f, filter)); // 200 Hz -> 20 kHz

    const int nCh = juce::jmin (buf.getNumChannels(), 2);
    const int n = buf.getNumSamples();
    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int s = 0; s < n; ++s)
    {
        const float f = freqSm.getNextValue();
        const float b = blendSm.getNextValue();
        const float cut = cutSm.getNextValue();
        const float g = 1.0f - std::exp (-(float) twoPi * cut / (float) sr);

        phase += f / sr;
        if (phase >= 1.0) phase -= 1.0;
        const float ph = (float) phase;

        const float sn = std::sin ((float) (twoPi * phase));
        const float cs = std::cos ((float) (twoPi * phase));
        const float tri = 4.0f * std::abs (ph - 0.5f) - 1.0f;
        const float sq = ph < 0.5f ? 1.0f : -1.0f;
        const float carrier = wave < 0.5f ? sn + (tri - sn) * (wave * 2.0f)
                                          : tri + (sq - tri) * ((wave - 0.5f) * 2.0f);

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& c = chans[ch];
            float* d = buf.getWritePointer (ch);
            const float x = d[s];
            float wet;
            if (mode == 0)
                wet = x * carrier;                    // ring modulation (RM)
            else
            {
                float a = x, q = x;
                for (int k = 0; k < 4; ++k) { a = c.p1[k].process (a); q = c.p2[k].process (q); }
                const float i = c.z1; c.z1 = a;       // 1-sample delay on path 1
                wet = (mode == 1) ? (i * cs - q * sn)  // upper sideband (UB)
                                  : (i * cs + q * sn); // lower sideband (LB)
            }
            c.lp1 += g * (wet - c.lp1);
            c.lp2 += g * (c.lp1 - c.lp2);
            d[s] = x * (1.0f - b) + c.lp2 * b;
        }
    }
}

juce::AudioProcessorEditor* RingProcessor::createEditor() { return new RingEditor (*this); }

void RingProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RingProcessor::setStateInformation (const void* data, int size)
{
    loading = true;
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    loading = false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RingProcessor(); }
