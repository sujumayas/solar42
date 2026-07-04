#include "PluginProcessor.h"
#include "PluginEditor.h"

Solar42NProcessor::Solar42NProcessor()
    : juce::AudioProcessor(BusesProperties()
          .withInput("Ext In", juce::AudioChannelSet::stereo(), true)   // EXT AUDIO / preamp source
          .withOutput("Wet Out", juce::AudioChannelSet::stereo(), true))
{
}

void Solar42NProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    rack_.prepare(sampleRate, samplesPerBlock);
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

void Solar42NProcessor::getStateInformation(juce::MemoryBlock&)
{
    // M7: full rig state (APVTS params + cables + keyboard + cartridges + unit serial).
}

void Solar42NProcessor::setStateInformation(const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Solar42NProcessor();
}
