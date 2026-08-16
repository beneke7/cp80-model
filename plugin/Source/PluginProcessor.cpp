#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
// The calibrated engine emits its raw common-voltage scale.  Offline renders
// normalise for comparison; the plugin needs a fixed line-output calibration.
constexpr float kLineOutputGain = 44.0f; // +32.9 dB, before no further limiting
}

juce::AudioProcessorValueTreeState::ParameterLayout
CP80PluginProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "volume", 1 }, "Volume", Range(0.0f, 1.0f, 0.001f), 0.8f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "bass", 1 }, "Bass", Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "middle", 1 }, "Middle", Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "treble", 1 }, "Treble", Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "brilliance", 1 }, "Brilliance",
        juce::StringArray { "Low", "Medium", "High" }, 1));
    p.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "tremolo", 1 }, "Tremolo", false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "depth", 1 }, "Depth", Range(15.0f, 45.0f, 0.1f), 30.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "speed", 1 }, "Speed", Range(0.8f, 10.0f, 0.01f), 5.5f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    return { p.begin(), p.end() };
}

CP80PluginProcessor::CP80PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("CP80State"), createParameterLayout())
{
    volumeParam = parameters.getRawParameterValue("volume");
    bassParam = parameters.getRawParameterValue("bass");
    middleParam = parameters.getRawParameterValue("middle");
    trebleParam = parameters.getRawParameterValue("treble");
    brillianceParam = parameters.getRawParameterValue("brilliance");
    tremoloParam = parameters.getRawParameterValue("tremolo");
    depthParam = parameters.getRawParameterValue("depth");
    speedParam = parameters.getRawParameterValue("speed");
}

void CP80PluginProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    piano.prepare(sampleRate, std::max(1, samplesPerBlock));
    tremoloPhase = 0.0f;
}

bool CP80PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet().isDisabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CP80PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() < 2)
        return;

    buffer.clear();
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    cp80::AdapterParameters values;
    values.volume = volumeParam->load(std::memory_order_relaxed);
    values.bassDb = bassParam->load(std::memory_order_relaxed);
    values.midDb = middleParam->load(std::memory_order_relaxed);
    values.trebleDb = trebleParam->load(std::memory_order_relaxed);
    values.brilliance = juce::jlimit(0, 2,
        int(std::lround(brillianceParam->load(std::memory_order_relaxed))));
    piano.setParameters(values);

    int cursor = 0;
    for (const auto metadata : midi) {
        const auto message = metadata.getMessage();
        cp80::AdapterEvent event;
        bool accepted = true;

        if (message.isNoteOn()) {
            event.type = cp80::AdapterEventType::NoteOn;
            event.note = message.getNoteNumber();
            event.value = message.getFloatVelocity();
        } else if (message.isNoteOff()) {
            event.type = cp80::AdapterEventType::NoteOff;
            event.note = message.getNoteNumber();
        } else if (message.isController() && message.getControllerNumber() == 64) {
            event.type = cp80::AdapterEventType::Sustain;
            event.value = message.getControllerValue() / 127.0f;
        } else {
            accepted = false;
        }
        if (!accepted)
            continue;

        const int offset = juce::jlimit(0, n, metadata.samplePosition);
        if (offset > cursor) {
            piano.process(left + cursor, offset - cursor);
            cursor = offset;
        }
        event.offset = 0;
        piano.process(nullptr, 0, &event, 1);
    }

    if (cursor < n)
        piano.process(left + cursor, n - cursor);

    renderTremolo(left, right, n);
}

void CP80PluginProcessor::renderTremolo(float* left, float* right, int numSamples)
{
    const bool enabled = tremoloParam->load(std::memory_order_relaxed) >= 0.5f;
    const float depth = juce::jlimit(0.15f, 0.45f,
        depthParam->load(std::memory_order_relaxed) * 0.01f);
    const float speed = juce::jlimit(0.8f, 10.0f,
                                     speedParam->load(std::memory_order_relaxed));
    const float inc = kTwoPi * speed / float(sampleRate);

    for (int i = 0; i < numSamples; ++i) {
        const float dry = left[i];
        if (!enabled) {
            right[i] = dry;
        } else {
            tremoloPhase += inc;
            if (tremoloPhase >= kTwoPi)
                tremoloPhase -= kTwoPi;
            const float c = std::cos(tremoloPhase);
            const float halfDepth = 0.5f * depth;
            const float centre = 1.0f - halfDepth;
            left[i] = dry * (centre + halfDepth * c);
            right[i] = dry * (centre - halfDepth * c);
        }
        left[i] *= kLineOutputGain;
        right[i] *= kLineOutputGain;
    }
}

void CP80PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);
    parameters.copyState().writeToStream(stream);
}

void CP80PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (state.isValid())
        parameters.replaceState(state);
}

juce::AudioProcessorEditor* CP80PluginProcessor::createEditor()
{
    return new CP80PluginEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CP80PluginProcessor();
}
