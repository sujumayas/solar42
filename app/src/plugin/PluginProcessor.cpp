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

    // Bottom modulation strip (manual pp10-12).
    const char* lfoIds[] = { "lfoA", "lfoB" };
    const char* lfoNames[] = { "LFO A", "LFO B" };
    for (int li = 0; li < 2; ++li)
    {
        const juce::String id(lfoIds[li]), name(lfoNames[li]);
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".wave", 1),
                                       name + " · Wave", range01, 0.5f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".rate", 1),
                                       name + " · Rate", range01, 0.4f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(id + ".range", 1), name + " · Range",
            juce::StringArray { "x1", "x6", "x10" }, 0));
    }
    layout.add(std::make_unique<P>(juce::ParameterID("joy.x", 1), "Joystick · X",
                                   juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f), 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("joy.y", 1), "Joystick · Y",
                                   juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f), 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("joy.xoff", 1), "Joystick · X Offset",
                                   juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f), 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("joy.yoff", 1), "Joystick · Y Offset",
                                   juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f), 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("seq.pulser", 1), "Seq · Pulser Rate", range01, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("seq.stages", 1), "Seq · Stages",
        juce::StringArray { "3", "4", "5" }, 2));
    for (int s = 1; s <= 5; ++s)
    {
        layout.add(std::make_unique<P>(juce::ParameterID("seq.step" + juce::String(s), 1),
                                       "Seq · Step " + juce::String(s), range01, 0.0f));
        layout.add(std::make_unique<B>(juce::ParameterID("seq.gate" + juce::String(s), 1),
                                       "Seq · Gate " + juce::String(s), true));
    }
    layout.add(std::make_unique<P>(juce::ParameterID("pre.gain", 1), "Preamp · Gain", range01, 0.25f));
    layout.add(std::make_unique<P>(juce::ParameterID("envf.att", 1), "Env Follower · Attack", range01, 0.2f));
    layout.add(std::make_unique<P>(juce::ParameterID("envf.rel", 1), "Env Follower · Release", range01, 0.4f));

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
    c.drone1Key = param("d1.gate") > 0.5f;

    static const float rangeMul[] = { 1.0f, 6.0f, 10.0f };
    c.lfoA.wave = param("lfoA.wave");
    c.lfoA.rateHz = expMap(param("lfoA.rate"), tn::kLfoAMin, tn::kLfoAMax)
                    * rangeMul[(int) param("lfoA.range") % 3];
    c.lfoB.wave = param("lfoB.wave");
    c.lfoB.rateHz = expMap(param("lfoB.rate"), tn::kLfoBMin, tn::kLfoBMax)
                    * rangeMul[(int) param("lfoB.range") % 3];

    c.joy.x = param("joy.x");
    c.joy.y = param("joy.y");
    c.joy.xOffset = param("joy.xoff");
    c.joy.yOffset = param("joy.yoff");

    c.seq.pulserHz = expMap(param("seq.pulser"), tn::kPulserMin, tn::kPulserMax);
    c.seq.stages = 3 + (int) param("seq.stages") % 3;
    for (int s = 0; s < 5; ++s)
    {
        c.seq.cv[s] = param(("seq.step" + juce::String(s + 1)).toRawUTF8()) * 5.0f;
        c.seq.gateOn[s] = param(("seq.gate" + juce::String(s + 1)).toRawUTF8()) > 0.5f;
    }

    c.preamp.gain = std::pow(100.0f, param("pre.gain")); // 0..40 dB
    c.preamp.attackSec = expMap(param("envf.att"), 0.001f, 0.5f);
    c.preamp.releaseSec = expMap(param("envf.rel"), 0.01f, 2.0f);

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
