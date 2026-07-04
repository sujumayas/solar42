#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Rack.h"
#include "state/PatchBay.h"

class Solar42NProcessor : public juce::AudioProcessor
{
public:
    Solar42NProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Solar42N"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; } // long reverb/drone tails

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }
    s42::Rack& rack() noexcept { return rack_; }
    solar::PatchBay& patchBay() noexcept { return patchBay_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    s42::Rack::Controls controlsFromParams() const noexcept;
    float param(const char* id) const noexcept;

    s42::Rack rack_;
    juce::AudioProcessorValueTreeState apvts_;
    solar::PatchBay patchBay_ { apvts_.state, rack_ };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NProcessor)
};
