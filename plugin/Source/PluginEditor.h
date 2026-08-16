#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class CP80PluginProcessor;

class CP80PluginEditor final : public juce::AudioProcessorEditor {
public:
    explicit CP80PluginEditor(CP80PluginProcessor&);
    ~CP80PluginEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    CP80PluginProcessor& processor;
    juce::Label title;
    juce::Label patch;
    juce::Slider volume, bass, middle, treble, depth, speed;
    juce::ComboBox brilliance;
    juce::ToggleButton tremolo { "Tremolo" };
    std::array<juce::Label, 8> labels;

    std::unique_ptr<SliderAttachment> volumeAttachment, bassAttachment,
        middleAttachment, trebleAttachment, depthAttachment, speedAttachment;
    std::unique_ptr<ComboAttachment> brillianceAttachment;
    std::unique_ptr<ButtonAttachment> tremoloAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CP80PluginEditor)
};

