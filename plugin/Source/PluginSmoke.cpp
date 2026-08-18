// Host-boundary smoke test: the processor produces finite audio, the tremolo is
// antiphase across the pair, and its two channels still sum to the dry signal.
//
// NOTE: do not use assert() here. CMake puts -DNDEBUG in every Release build, which
// compiles asserts away and leaves a test that passes by doing nothing.
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
int failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}

juce::MidiBuffer noteOn()
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
    return midi;
}
} // namespace

int main()
{
    constexpr int n = 256;

    // Heap, not stack: CP80PluginProcessor embeds a ~890 kB cp80::CP80, and two of
    // them overflow the 1 MB default stack on Windows.
    auto dryProcessor = std::make_unique<CP80PluginProcessor>();
    auto tremProcessor = std::make_unique<CP80PluginProcessor>();
    dryProcessor->prepareToPlay(48000.0, n);
    tremProcessor->prepareToPlay(48000.0, n);
    tremProcessor->parameters.getParameter("tremolo")->setValueNotifyingHost(1.0f);

    juce::AudioBuffer<float> dry(2, n), trem(2, n);
    auto dryMidi = noteOn();
    auto tremMidi = noteOn();
    dryProcessor->processBlock(dry, dryMidi);
    tremProcessor->processBlock(trem, tremMidi);

    float maxOffDifference = 0.0f;
    float maxSumError = 0.0f;
    float maxStereoDifference = 0.0f;
    float maxDryLevel = 0.0f;
    bool allFinite = true;
    constexpr float expectedSumScale = 2.0f - 0.30f;

    for (int i = 0; i < n; ++i) {
        const float l0 = dry.getSample(0, i);
        const float l1 = dry.getSample(1, i);
        const float tl = trem.getSample(0, i);
        const float tr = trem.getSample(1, i);
        allFinite = allFinite && std::isfinite(l0) && std::isfinite(l1)
                              && std::isfinite(tl) && std::isfinite(tr);
        maxDryLevel = std::max(maxDryLevel, std::fabs(l0));
        maxOffDifference = std::max(maxOffDifference, std::fabs(l0 - l1));
        maxSumError = std::max(maxSumError, std::fabs((tl + tr) - l0 * expectedSumScale));
        maxStereoDifference = std::max(maxStereoDifference, std::fabs(tl - tr));
    }

    check(allFinite, "output is finite");
    check(maxOffDifference < 1.0e-6f, "tremolo off leaves the channels identical");
    check(maxDryLevel > 0.01f, "a note actually sounds");
    check(maxSumError < 2.0e-5f, "tremolo channels sum back to the dry signal");
    check(maxStereoDifference > 1.0e-7f, "tremolo is antiphase across the pair");

    std::printf("dry peak %.4f  off-diff %.2e  sum-err %.2e  stereo-diff %.2e\n",
                maxDryLevel, maxOffDifference, maxSumError, maxStereoDifference);
    std::printf(failures == 0 ? "smoke: OK\n" : "smoke: %d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
