#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

#include "cp80_adapter.hpp"

class CP80PluginProcessor final : public juce::AudioProcessor {
public:
    CP80PluginProcessor();
    ~CP80PluginProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CP-80"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    // Editor chrome, not a parameter: hosts should restore it but not automate it.
    bool plateVisible = true;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void renderTremolo(float* left, float* right, int numSamples);

    cp80::CP80Adapter piano;
    double sampleRate = 48000.0;
    float tremoloPhase = 0.0f;

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* bassParam = nullptr;
    std::atomic<float>* middleParam = nullptr;
    std::atomic<float>* trebleParam = nullptr;
    std::atomic<float>* brillianceParam = nullptr;
    std::atomic<float>* tremoloParam = nullptr;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* speedParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CP80PluginProcessor)
};
