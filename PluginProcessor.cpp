#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    const double hA1[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    const double hA2[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };

    float polyBlep (float t, float dt)
    {
        if (t < dt)         { t /= dt;               return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)  { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // Five waveforms crossfaded by one knob: square > sine > ramp down | ramp up > triangle
    float morphWave (float ph, float dt, float w)
    {
        dt = juce::jlimit (0.00001f, 0.49f, dt);
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float sine = std::sin (twoPi * ph);
        if (w < 0.25f)
        {
            float sq = ph < 0.5f ? 1.0f : -1.0f;
            float ph2 = ph + 0.5f; ph2 -= std::floor (ph2);
            sq += polyBlep (ph, dt) - polyBlep (ph2, dt);
            const float t = w * 4.0f;
            return sq * (1.0f - t) + sine * t;
        }
        if (w < 0.5f)
        {
            const float rd = 1.0f - 2.0f * ph + polyBlep (ph, dt);
            const float t = (w - 0.25f) * 4.0f;
            return sine * (1.0f - t) + rd * t;
        }
        const float ru = 2.0f * ph - 1.0f - polyBlep (ph, dt);
        const float tri = 4.0f * std::abs (ph - 0.5f) - 1.0f;
        const float t = (w - 0.5f) * 2.0f;
        return ru * (1.0f - t) + tri * t;
    }
}

const char* const RingProcessor::factoryNames[9] = {
    "Ring Mod", "Tremolo", "Ring Vibe", "Detune", "Chorus", "Vibrato", "Sub Octave", "Octave Up", "Rotary Thing" };

PresetData RingProcessor::factoryPreset (int i)
{
    //                       blend  wave   filter fine   coarse modes
    static const PresetData t[9] = {
        { 0.60f, 0.25f, 0.80f, 0.50f, 0.80f, kRM, 0.0f },   // Ring Mod
        { 0.50f, 0.25f, 1.00f, 0.50f, 0.38f, kRM, 0.0f },   // Tremolo
        { 0.50f, 0.50f, 1.00f, 0.50f, 0.35f, kLB, 0.0f },   // Ring Vibe
        { 0.50f, 0.25f, 0.00f, 0.60f, 0.50f, kPS, 0.0f },   // Detune
        { 0.50f, 0.25f, 0.30f, 0.15f, 0.50f, kPS, 0.0f },   // Chorus
        { 1.00f, 0.25f, 0.50f, 0.25f, 0.50f, kPS, 0.0f },   // Vibrato
        { 0.60f, 0.25f, 0.00f, 0.50f, 0.25f, kPS, 0.0f },   // Sub Octave (-12 st)
        { 0.50f, 0.25f, 0.00f, 0.50f, 0.75f, kPS, 0.0f },   // Octave Up (+12 st)
        { 0.70f, 0.25f, 0.50f, 0.35f, 0.50f, kPS, 0.0f } }; // Rotary Thing
    return t[juce::jlimit (0, 8, i)];
}

juce::AudioProcessorValueTreeState::ParameterLayout RingProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto f = [&p] (const char* id, const char* name, float def) {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def));
    };
    f ("blend", "Blend", 0.5f);
    f ("wave", "Wave", 0.25f);
    f ("filter", "Filter/Rate", 1.0f);
    f ("fine", "Fine/Depth", 0.5f);
    f ("coarse", "Coarse", 0.5f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "rm", 1 }, "Mode RM", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "ub", 1 }, "Mode UB", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "lb", 1 }, "Mode LB", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "ps", 1 }, "Mode Pitch Shift", false));
    p.push_back (std::make_unique<AudioParameterInt>  (ParameterID { "preset", 1 }, "Preset", 0, 9, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "active", 1 }, "Effect On (Bypass switch)", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "presetfsw", 1 }, "Preset/Tune Footswitch", false));
    return { p.begin(), p.end() };
}

RingProcessor::RingProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pBlend = apvts.getRawParameterValue ("blend");   pWave = apvts.getRawParameterValue ("wave");
    pFilter = apvts.getRawParameterValue ("filter"); pFine = apvts.getRawParameterValue ("fine");
    pCoarse = apvts.getRawParameterValue ("coarse"); pRM = apvts.getRawParameterValue ("rm");
    pUB = apvts.getRawParameterValue ("ub");         pLB = apvts.getRawParameterValue ("lb");
    pPS = apvts.getRawParameterValue ("ps");         pActive = apvts.getRawParameterValue ("active");
    pFsw = apvts.getRawParameterValue ("presetfsw"); pPreset = apvts.getRawParameterValue ("preset");

    for (int i = 0; i < 9; ++i) userPresets[i] = factoryPreset (i);
    loadUserPresetsFromDisk();
    apvts.addParameterListener ("preset", this);
}

RingProcessor::~RingProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener ("preset", this);
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
    for (auto& c : chans)
    {
        c = Chan();
        for (int k = 0; k < 4; ++k) { c.p1[k].a2 = (float) (hA1[k] * hA1[k]); c.p2[k].a2 = (float) (hA2[k] * hA2[k]); }
    }
    psWin = juce::jmax (256, (int) (0.04 * sr));
    const int size = juce::nextPowerOfTwo (psWin + 16);
    for (auto& b : psBuf) b.assign ((size_t) size, 0.0f);
    psMask = size - 1; psPos = 0; psPhase = 0.f;

    tuneBuf.assign ((size_t) tuneSize, 0.0f);
    tuneWork.assign ((size_t) tuneSize, 0.0f);
    yinD.assign ((size_t) 3100, 0.0f);
    tuneWrite = 0; tuneCounter = 0;

    freqSm.reset (sr, 0.03);  freqSm.setCurrentAndTargetValue (10.0f);
    blendSm.reset (sr, 0.02); blendSm.setCurrentAndTargetValue (pBlend->load());
    cutSm.reset (sr, 0.02);   cutSm.setCurrentAndTargetValue (20000.0f);
    psBaseSm.reset (sr, 0.03); psBaseSm.setCurrentAndTargetValue (0.0f);
    depthSm.reset (sr, 0.03);  depthSm.setCurrentAndTargetValue (0.0f);
    activeSm.reset (sr, 0.01); activeSm.setCurrentAndTargetValue (pActive->load() > 0.5f ? 1.0f : 0.0f);
}

int RingProcessor::getModes() const
{
    return (pRM->load() > 0.5f ? kRM : 0) | (pUB->load() > 0.5f ? kUB : 0)
         | (pLB->load() > 0.5f ? kLB : 0) | (pPS->load() > 0.5f ? kPS : 0);
}

// ------------------------------------------------------------------ MIDI
void RingProcessor::handleMidi (juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const auto learn = (MidiTarget) learnTarget.load();
        if (learn != MidiTarget::None && (m.isController() || m.isNoteOn()))
        {
            const int cc   = m.isController() ? m.getControllerNumber() : -1;
            const int note = m.isNoteOn() ? m.getNoteNumber() : -1;
            if (learn == MidiTarget::PresetFsw) { ccPreset = cc; notePreset = note; }
            else                                { ccBypass = cc; noteBypass = note; }
            learnTarget = 0;
            continue;
        }
        if (m.isController())
        {
            const int c = m.getControllerNumber();
            const bool on = m.getControllerValue() >= 64;
            if (c == ccPreset.load()) midiPresetDown = on;
            if (c == ccBypass.load())
            {
                if (on && ! midiBypassHeld.load()) { pendingToggle = true; triggerAsyncUpdate(); }
                midiBypassHeld = on;
            }
        }
        else if (m.isNoteOn())
        {
            if (m.getNoteNumber() == notePreset.load()) midiPresetDown = true;
            if (m.getNoteNumber() == noteBypass.load()) { pendingToggle = true; triggerAsyncUpdate(); }
        }
        else if (m.isNoteOff())
        {
            if (m.getNoteNumber() == notePreset.load()) midiPresetDown = false;
        }
    }
}

void RingProcessor::clearMidi (MidiTarget t)
{
    if (t == MidiTarget::PresetFsw) { ccPreset = -1; notePreset = -1; midiPresetDown = false; }
    else                            { ccBypass = -1; noteBypass = -1; }
    if (getLearnTarget() == t) learnTarget = 0;
}

juce::String RingProcessor::getMidiDescription (MidiTarget t) const
{
    const int cc   = t == MidiTarget::PresetFsw ? ccPreset.load()   : ccBypass.load();
    const int note = t == MidiTarget::PresetFsw ? notePreset.load() : noteBypass.load();
    if (cc >= 0)   return "CC " + juce::String (cc);
    if (note >= 0) return "Note " + juce::String (note);
    return {};
}

// ------------------------------------------------------------------ pitch detection (YIN)
float RingProcessor::detectPitch()
{
    const int N = tuneSize, W = 1024;
    const int tauMax = juce::jmin ((int) (sr / 40.0), 3000, N - W - 1);
    const int tauMin = juce::jmax (2, (int) (sr / 1500.0));
    if (tauMax <= tauMin + 2) return 0.0f;

    for (int i = 0; i < N; ++i) tuneWork[(size_t) i] = tuneBuf[(size_t) ((tuneWrite - N + i) & (N - 1))];
    const float* x = tuneWork.data() + (N - W - tauMax);

    double energy = 0.0;
    for (int j = 0; j < W; ++j) energy += (double) x[tauMax + j] * x[tauMax + j];
    if (std::sqrt (energy / W) < 0.003) return 0.0f;

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        const float* a = x + tauMax;      // newest window
        const float* b = x + tauMax - tau;
        for (int j = 0; j < W; ++j) { const float d = a[j] - b[j]; sum += (double) d * d; }
        yinD[(size_t) tau] = (float) sum;
    }
    // cumulative mean normalised difference
    double running = 0.0;
    std::vector<float>& d = yinD;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += d[(size_t) tau];
        d[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (d[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
            best = tau; break;
        }
    }
    if (best < 0) return 0.0f;
    float t = (float) best;
    if (best > 1 && best < tauMax)
    {
        const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
        const float den = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs (den) > 1e-9f) t += (s2 - s0) / den;
    }
    const float f0 = (float) sr / t;
    return (f0 >= 30.0f && f0 <= 1500.0f) ? f0 : 0.0f;
}

// ------------------------------------------------------------------ audio
void RingProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    const int nCh = juce::jmin (buf.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buf.clear (c, 0, n);

    handleMidi (midi);

    // PRESET/TUNE footswitch: short press = next preset, hold >= 1 s = continuous tune
    const bool down = pFsw->load() > 0.5f || midiPresetDown.load();
    if (down)
    {
        holdSamples += n;
        if (! tuning.load() && holdSamples >= sr) tuning = true;
    }
    else
    {
        if (prevDown && ! tuning.load()) { pendingNext = true; triggerAsyncUpdate(); }
        holdSamples = 0.0; tuning = false;
    }
    prevDown = down;

    const bool active = pActive->load() > 0.5f;
    activeSm.setTargetValue (active ? 1.0f : 0.0f);
    if (! active && ! activeSm.isSmoothing()) return;

    const float blend = pBlend->load(), wave = pWave->load(), filter = pFilter->load();
    const float fine = pFine->load(), coarse = pCoarse->load();
    const int modes = getModes();
    const bool rm = (modes & kRM) != 0, ub = (modes & kUB) != 0, lb = (modes & kLB) != 0, ps = (modes & kPS) != 0;
    const bool ssb = ub || lb, ringAny = rm || ssb;

    if (tunedActive.load() && (std::abs (coarse - tunedCoarse) > 1e-3f || std::abs (fine - tunedFine) > 1e-3f))
        tunedActive = false;

    const float fHz = tunedActive.load() ? tunedHz.load()
                                         : 0.1f * std::pow (23900.0f, coarse) * (1.0f + (fine - 0.5f) * 0.66f);
    freqSm.setTargetValue (juce::jlimit (0.05f, 4000.0f, fHz));
    blendSm.setTargetValue (blend);
    const float cutoff = juce::jmin (20.0f * std::pow (1000.0f, filter), (float) (0.45 * sr));
    cutSm.setTargetValue (cutoff);
    const bool lpfOn = filter < 0.985f;

    const float lfoRate = (ps && filter > 0.02f) ? 0.1f * std::pow (5000.0f, (filter - 0.02f) / 0.98f) : 0.0f;
    const bool modOn = lfoRate > 0.0f;
    psBaseSm.setTargetValue (std::round ((coarse - 0.5f) * 48.0f) + (modOn ? 0.0f : (fine - 0.5f) * 2.0f));
    depthSm.setTargetValue (modOn ? fine * 1.5f : 0.0f);

    float w2 = 0.0f, w3 = 0.0f;
    if (wave < 0.5f) w2 = (0.5f - wave) * 2.0f; else w3 = (wave - 0.5f) * 2.0f;
    const float hNorm = 1.0f / (1.0f + 0.5f * (w2 + w3));

    float* ch[2] = { buf.getWritePointer (0), nCh > 1 ? buf.getWritePointer (1) : nullptr };
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float fs = (float) sr;

    for (int s = 0; s < n; ++s)
    {
        const float f = freqSm.getNextValue();
        const float b = blendSm.getNextValue();
        const float g = 1.0f - std::exp (-twoPi * cutSm.getNextValue() / fs);
        const float act = activeSm.getNextValue();
        const float baseS = psBaseSm.getNextValue();
        const float dep = depthSm.getNextValue();

        float mono = 0.0f;
        for (int c = 0; c < nCh; ++c) mono += ch[c][s];
        tuneBuf[(size_t) (tuneWrite & (tuneSize - 1))] = mono / (float) nCh;
        ++tuneWrite;

        const float dt = f / fs;
        carrierPhase += dt; carrierPhase -= std::floor (carrierPhase);
        lfoPhase += lfoRate / fs; lfoPhase -= std::floor (lfoPhase);

        const float rmCar = rm ? morphWave (carrierPhase, dt, wave) : 0.0f;
        float cH = 0.0f, sH = 0.0f;
        if (ssb)
        {
            const float a = twoPi * carrierPhase;
            cH = (std::cos (a) + w2 * std::cos (2.0f * a) + w3 * std::cos (3.0f * a)) * hNorm;
            sH = (std::sin (a) + w2 * std::sin (2.0f * a) + w3 * std::sin (3.0f * a)) * hNorm;
        }

        float g1 = 0.0f, g2 = 0.0f, d1 = 2.0f, d2 = 2.0f;
        if (ps)
        {
            const float lfoV = modOn ? morphWave (lfoPhase, lfoRate / fs, wave) : 0.0f;
            const float ratio = std::pow (2.0f, (baseS + dep * lfoV) / 12.0f);
            psPhase += (1.0f - ratio) / (float) psWin; psPhase -= std::floor (psPhase);
            float p2 = psPhase + 0.5f; p2 -= std::floor (p2);
            d1 = 2.0f + psPhase * (float) psWin;  d2 = 2.0f + p2 * (float) psWin;
            g1 = 0.5f - 0.5f * std::cos (twoPi * psPhase);
            g2 = 0.5f - 0.5f * std::cos (twoPi * p2);
        }

        for (int c = 0; c < nCh; ++c)
        {
            auto& st = chans[c];
            const float x = ch[c][s];

            float ringSum = 0.0f; int nRing = 0;
            if (rm) { ringSum += x * rmCar; ++nRing; }
            if (ssb)
            {
                float a = x, q = x;
                for (int k = 0; k < 4; ++k) { a = st.p1[k].process (a); q = st.p2[k].process (q); }
                const float i = st.z1; st.z1 = a;
                if (ub) { ringSum += i * cH - q * sH; ++nRing; }
                if (lb) { ringSum += i * cH + q * sH; ++nRing; }
            }
            float ring = nRing > 0 ? ringSum / (float) nRing : 0.0f;
            if (ringAny && lpfOn)
            {
                st.l1 += g * (ring - st.l1);
                st.l2 += g * (st.l1 - st.l2);
                st.l3 += g * (st.l2 - st.l3);
                ring = st.l3;
            }

            float psOut = 0.0f;
            auto& pb = psBuf[c];
            pb[(size_t) psPos] = x;
            if (ps)
            {
                auto rd = [&] (float delay) {
                    const float rp = (float) psPos - delay;
                    const int i0 = (int) std::floor (rp);
                    const float fr = rp - (float) i0;
                    const float va = pb[(size_t) (i0 & psMask)], vb = pb[(size_t) ((i0 + 1) & psMask)];
                    return va + fr * (vb - va);
                };
                psOut = rd (d1) * g1 + rd (d2) * g2;
            }

            const int count = (ringAny ? 1 : 0) + (ps ? 1 : 0);
            const float wet = count == 0 ? x : (ring + psOut) / std::sqrt ((float) count);
            const float processed = x * (1.0f - b) + wet * b;
            ch[c][s] = x + (processed - x) * act;
        }
        psPos = (psPos + 1) & psMask;
    }

    // continuous tuning of the ring-modulator carrier to the incoming note
    if (tuning.load() && (modes & (kRM | kUB | kLB)) != 0)
    {
        tuneCounter += n;
        if (tuneCounter >= 1024)
        {
            tuneCounter = 0;
            const float f0 = detectPitch();
            if (f0 > 0.0f)
            {
                tunedHz = f0; tunedActive = true;
                tunedCoarse = coarse; tunedFine = fine;
            }
        }
    }
}

// ------------------------------------------------------------------ presets
void RingProcessor::setParamValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

PresetData RingProcessor::captureCurrent() const
{
    PresetData d;
    d.blend = pBlend->load(); d.wave = pWave->load(); d.filter = pFilter->load();
    d.fine = pFine->load();   d.coarse = pCoarse->load(); d.modes = getModes();
    d.tunedHz = tunedActive.load() ? tunedHz.load() : 0.0f;
    return d;
}

void RingProcessor::applyPresetData (const PresetData& p)
{
    setParamValue ("blend", p.blend);  setParamValue ("wave", p.wave);   setParamValue ("filter", p.filter);
    setParamValue ("fine", p.fine);    setParamValue ("coarse", p.coarse);
    setParamValue ("rm", (p.modes & kRM) ? 1.0f : 0.0f); setParamValue ("ub", (p.modes & kUB) ? 1.0f : 0.0f);
    setParamValue ("lb", (p.modes & kLB) ? 1.0f : 0.0f); setParamValue ("ps", (p.modes & kPS) ? 1.0f : 0.0f);
    tunedCoarse = p.coarse; tunedFine = p.fine;
    tunedHz = p.tunedHz;
    tunedActive = p.tunedHz > 0.0f;
}

void RingProcessor::loadPreset (int slot)
{
    slot = juce::jlimit (0, 9, slot);
    currentSlot = slot;
    setParamValue ("preset", (float) slot);
    if (slot > 0) applyPresetData (userPresets[slot - 1]);
}

void RingProcessor::requestNextPreset()
{
    loadPreset (currentSlot >= 9 ? 0 : currentSlot + 1);   // after preset 9: live knobs, then preset 1 again
}

bool RingProcessor::isPresetDirty() const
{
    if (currentSlot < 1) return false;
    const auto& p = userPresets[currentSlot - 1];
    const auto c = captureCurrent();
    auto d = [] (float a, float b) { return std::abs (a - b) > 0.002f; };
    return d (c.blend, p.blend) || d (c.wave, p.wave) || d (c.filter, p.filter)
        || d (c.fine, p.fine) || d (c.coarse, p.coarse) || c.modes != p.modes;
}

void RingProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return;
    userPresets[slot - 1] = captureCurrent();
    writeUserPresetsToDisk();
    lastSaveMs = juce::Time::getMillisecondCounter();
}

void RingProcessor::restoreFactoryPreset (int slot)
{
    if (slot < 1 || slot > 9) return;
    userPresets[slot - 1] = factoryPreset (slot - 1);
    writeUserPresetsToDisk();
    if (currentSlot == slot) applyPresetData (userPresets[slot - 1]);
}

void RingProcessor::restoreAllFactory()
{
    for (int i = 0; i < 9; ++i) userPresets[i] = factoryPreset (i);
    writeUserPresetsToDisk();
    if (currentSlot > 0) applyPresetData (userPresets[currentSlot - 1]);
}

void RingProcessor::cycleModes()
{
    static const int order[] = { kRM, kUB, kLB, kRM | kUB, kUB | kLB, kRM | kLB, kRM | kUB | kLB, kPS };
    const int cur = getModes();
    int next = order[0];
    for (int i = 0; i < 8; ++i) if (order[i] == cur) { next = order[(i + 1) % 8]; break; }
    setParamValue ("rm", (next & kRM) ? 1.0f : 0.0f); setParamValue ("ub", (next & kUB) ? 1.0f : 0.0f);
    setParamValue ("lb", (next & kLB) ? 1.0f : 0.0f); setParamValue ("ps", (next & kPS) ? 1.0f : 0.0f);
}

juce::File RingProcessor::getPresetFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("WigaRingThink");
    dir.createDirectory();
    return dir.getChildFile ("user_presets.xml");
}

void RingProcessor::loadUserPresetsFromDisk()
{
    auto f = getPresetFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::parseXML (f))
        for (auto* e : xml->getChildIterator())
        {
            const int slot = e->getIntAttribute ("slot", 0);
            if (slot < 1 || slot > 9) continue;
            auto& p = userPresets[slot - 1];
            p.blend = (float) e->getDoubleAttribute ("blend", p.blend);
            p.wave = (float) e->getDoubleAttribute ("wave", p.wave);
            p.filter = (float) e->getDoubleAttribute ("filter", p.filter);
            p.fine = (float) e->getDoubleAttribute ("fine", p.fine);
            p.coarse = (float) e->getDoubleAttribute ("coarse", p.coarse);
            p.modes = e->getIntAttribute ("modes", p.modes);
            p.tunedHz = (float) e->getDoubleAttribute ("tunedHz", 0.0);
        }
}

void RingProcessor::writeUserPresetsToDisk()
{
    juce::XmlElement root ("PRESETS");
    for (int i = 0; i < 9; ++i)
    {
        auto* e = root.createNewChildElement ("P");
        const auto& p = userPresets[i];
        e->setAttribute ("slot", i + 1);
        e->setAttribute ("blend", (double) p.blend); e->setAttribute ("wave", (double) p.wave);
        e->setAttribute ("filter", (double) p.filter); e->setAttribute ("fine", (double) p.fine);
        e->setAttribute ("coarse", (double) p.coarse); e->setAttribute ("modes", p.modes);
        e->setAttribute ("tunedHz", (double) p.tunedHz);
    }
    getPresetFile().replaceWithText (root.toString());
}

void RingProcessor::parameterChanged (const juce::String& id, float v)
{
    if (id == "preset") { pendingPreset = (int) std::lround (v); triggerAsyncUpdate(); }
}

void RingProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index > 8) return;
    currentProgram = index;
    pendingProgram = index;
    triggerAsyncUpdate();
}

void RingProcessor::handleAsyncUpdate()
{
    if (pendingToggle.exchange (false))
        setParamValue ("active", pActive->load() > 0.5f ? 0.0f : 1.0f);
    if (pendingNext.exchange (false))
        requestNextPreset();
    const int prog = pendingProgram.exchange (-1);
    if (prog >= 0)
    {
        currentSlot = 0; setParamValue ("preset", 0.0f);
        applyPresetData (factoryPreset (prog));
    }
    const int v = pendingPreset.exchange (-1);
    if (v >= 0 && v != currentSlot) loadPreset (v);
}

// ------------------------------------------------------------------ state
juce::AudioProcessorEditor* RingProcessor::createEditor() { return new RingEditor (*this); }

void RingProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    apvts.state.setProperty ("ccPreset", ccPreset.load(), nullptr);
    apvts.state.setProperty ("notePreset", notePreset.load(), nullptr);
    apvts.state.setProperty ("ccBypass", ccBypass.load(), nullptr);
    apvts.state.setProperty ("noteBypass", noteBypass.load(), nullptr);
    apvts.state.setProperty ("tunedHz", (double) tunedHz.load(), nullptr);
    apvts.state.setProperty ("tunedActive", tunedActive.load(), nullptr);
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RingProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            ccPreset = (int) apvts.state.getProperty ("ccPreset", -1);
            notePreset = (int) apvts.state.getProperty ("notePreset", -1);
            ccBypass = (int) apvts.state.getProperty ("ccBypass", -1);
            noteBypass = (int) apvts.state.getProperty ("noteBypass", -1);
            tunedHz = (float) (double) apvts.state.getProperty ("tunedHz", 0.0);
            tunedActive = (bool) apvts.state.getProperty ("tunedActive", false);
            tunedCoarse = pCoarse->load(); tunedFine = pFine->load();
            currentSlot = (int) std::lround (pPreset->load());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RingProcessor(); }
