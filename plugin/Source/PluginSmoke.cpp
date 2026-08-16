#include "PluginProcessor.h"

#include <algorithm>
#include <cassert>
#include <cmath>

static juce::MidiBuffer noteOn()
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
    return midi;
}

int main()
{
    constexpr int n = 256;
    CP80PluginProcessor dryProcessor;
    CP80PluginProcessor tremProcessor;
    dryProcessor.prepareToPlay(48000.0, n);
    tremProcessor.prepareToPlay(48000.0, n);
    tremProcessor.parameters.getParameter("tremolo")->setValueNotifyingHost(1.0f);

    juce::AudioBuffer<float> dry(2, n), trem(2, n);
    auto dryMidi = noteOn();
    auto tremMidi = noteOn();
    dryProcessor.processBlock(dry, dryMidi);
    tremProcessor.processBlock(trem, tremMidi);

    float maxOffDifference = 0.0f;
    float maxSumError = 0.0f;
    float maxStereoDifference = 0.0f;
    constexpr float expectedSumScale = 2.0f - 0.30f;
    for (int i = 0; i < n; ++i) {
        const float l0 = dry.getSample(0, i);
        const float l1 = dry.getSample(1, i);
        const float tl = trem.getSample(0, i);
        const float tr = trem.getSample(1, i);
        assert(std::isfinite(l0) && std::isfinite(l1));
        assert(std::isfinite(tl) && std::isfinite(tr));
        maxOffDifference = std::max(maxOffDifference, std::fabs(l0 - l1));
        maxSumError = std::max(maxSumError, std::fabs((tl + tr) - l0 * expectedSumScale));
        maxStereoDifference = std::max(maxStereoDifference, std::fabs(tl - tr));
    }
    assert(maxOffDifference < 1.0e-6f);
    assert(maxSumError < 2.0e-5f);
    assert(maxStereoDifference > 1.0e-7f);
    return 0;
}
