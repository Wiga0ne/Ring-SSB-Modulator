#pragma once
#include "PluginProcessor.h"

class PedalContent;

class RingEditor : public juce::AudioProcessorEditor
{
public:
    explicit RingEditor (RingProcessor&);
    ~RingEditor() override;
    void resized() override;
    int  getScaleIndex() const;          // -1 if the size is not one of the 3 menu sizes
    void setScaleIndex (int idx);        // 0..2 = 50 %, 75 %, 100 %  (-1 = default 60 %)

private:
    RingProcessor& proc;
    std::unique_ptr<PedalContent> content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingEditor)
};
