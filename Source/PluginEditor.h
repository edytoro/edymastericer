#pragma once

#include "PluginProcessor.h"
#include <array>
#include <vector>

class EdyMastericerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit EdyMastericerAudioProcessorEditor (EdyMastericerAudioProcessor&);
    ~EdyMastericerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kNumKnobs = 17;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;

    EdyMastericerAudioProcessor& proc;

    std::array<Knob, kNumKnobs> knobs;
    juce::ToggleButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::vector<juce::Rectangle<int>> sectionBounds;
    juce::Rectangle<int> meterArea;
    float display[4] { 0.0f, 0.0f, 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EdyMastericerAudioProcessorEditor)
};
