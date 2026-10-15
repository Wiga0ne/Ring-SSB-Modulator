#pragma once
#include "PluginProcessor.h"

class PedalContent;

class RingEditor : public juce::AudioProcessorEditor
{
public:
    explicit RingEditor (RingProcessor&);
    ~RingEditor() override;
    void resized() override;
    int  getScaleIndex() const;          // -1 if the size is not one of the 4 presets
    void setScaleIndex (int idx);        // 0..3 = 25 %, 50 %, 75 %, 100 %

private:
    RingProcessor& proc;
    std::unique_ptr<PedalContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingEditor)
};
