#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/TuningConstants.h"

namespace
{
float expMap(float v01, float lo, float hi) noexcept
{
    return lo * std::pow(hi / lo, v01);
}
} // namespace

Solar42NProcessor::Solar42NProcessor()
    : juce::AudioProcessor(BusesProperties()
          .withInput("Ext In", juce::AudioChannelSet::stereo(), true)   // EXT AUDIO / preamp source
          .withOutput("Wet Out", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Solar42NProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto range01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f);

    // DRONE 1 (M1). IDs follow the final section.control scheme and stay stable.
    const float tuneDefaults[] = { 0.45f, 0.52f, 0.30f, 0.50f, 0.50f };
    for (int g = 0; g < s42::ClassicDroneVoice::kNumGens; ++g)
    {
        const auto n = juce::String(g + 1);
        layout.add(std::make_unique<P>(juce::ParameterID("d1.gen" + n + ".tune", 1),
                                       "Drone 1 · Gen " + n + " Tune", range01, tuneDefaults[g]));
        layout.add(std::make_unique<B>(juce::ParameterID("d1.gen" + n + ".mute", 1),
                                       "Drone 1 · Gen " + n + " Mute", g >= 3));
    }
    layout.add(std::make_unique<P>(juce::ParameterID("d1.volt", 1), "Drone 1 · Volt", range01, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("d1.att", 1), "Drone 1 · Attack", range01, 0.45f));
    layout.add(std::make_unique<P>(juce::ParameterID("d1.rls", 1), "Drone 1 · Release", range01, 0.6f));
    layout.add(std::make_unique<B>(juce::ParameterID("d1.hold", 1), "Drone 1 · Hold", false));
    layout.add(std::make_unique<B>(juce::ParameterID("d1.gate", 1), "Drone 1 · Gate (Keypad 1)", false));

    // Placeholder filter + master (final Polivoks bank in M3).
    layout.add(std::make_unique<P>(juce::ParameterID("filt.freq", 1), "Filter · Freq", range01, 0.55f));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.res", 1), "Filter · Res", range01, 0.30f));
    layout.add(std::make_unique<P>(juce::ParameterID("master.vol", 1), "Master · Volume", range01, 0.70f));

    return layout;
}

float Solar42NProcessor::param(const char* id) const noexcept
{
    if (auto* raw = apvts_.getRawParameterValue(id))
        return raw->load();
    return 0.0f;
}

s42::Rack::Controls Solar42NProcessor::controlsFromParams() const noexcept
{
    namespace tn = s42::tuning;
    s42::Rack::Controls c;

    for (int g = 0; g < s42::ClassicDroneVoice::kNumGens; ++g)
    {
        const auto base = "d1.gen" + juce::String(g + 1);
        c.drone1.tune[g] = param((base + ".tune").toRawUTF8());
        c.drone1.mute[g] = param((base + ".mute").toRawUTF8()) > 0.5f;
    }
    c.drone1.volt = param("d1.volt");
    c.drone1.attackSec = expMap(param("d1.att"), tn::kDroneAttMin, tn::kDroneAttMax);
    c.drone1.releaseSec = expMap(param("d1.rls"), tn::kDroneRlsMin, tn::kDroneRlsMax);
    c.drone1.hold = param("d1.hold") > 0.5f;
    c.drone1Gate = param("d1.gate") > 0.5f;

    c.filterFreqHz = expMap(param("filt.freq"), tn::kFilterFreqMin, tn::kFilterFreqMax);
    c.filterRes = param("filt.res");
    const float mv = param("master.vol");
    c.masterVol = mv * mv; // audio taper

    return c;
}

void Solar42NProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    rack_.prepare(sampleRate, samplesPerBlock);
    rack_.setControls(controlsFromParams());
}

bool Solar42NProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            || layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled());
}

void Solar42NProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    rack_.setControls(controlsFromParams());

    const auto numSamples = buffer.getNumSamples();
    const bool hasInput = getTotalNumInputChannels() >= 2;
    const float* extL = hasInput ? buffer.getReadPointer(0) : nullptr;
    const float* extR = hasInput ? buffer.getReadPointer(1) : nullptr;

    rack_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), extL, extR, numSamples);

    for (int ch = 2; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

juce::AudioProcessorEditor* Solar42NProcessor::createEditor()
{
    return new Solar42NEditor(*this);
}

void Solar42NProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // M7 adds cables/keyboard/cartridges/unit-serial; params round-trip already.
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void Solar42NProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Solar42NProcessor();
}
