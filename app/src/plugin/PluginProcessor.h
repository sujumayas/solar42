#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "engine/Rack.h"
#include "engine/SeqlockBox.h"
#include "engine/keyboard/MidiPerformer.h"
#include "state/KeyboardState.h"
#include "state/PatchBay.h"

namespace solar {
class PresetManager;
}

class Solar42NProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    Solar42NProcessor();
    ~Solar42NProcessor() override;

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

    // ---- M7 state & preset plumbing (message thread unless noted).

    // The full rig state as saved: params + CABLES + KEYBOARD + CARTRIDGES +
    // TOLERANCES (+ UI) — the .s42n payload. (Non-const: copyState() flushes
    // pending parameter values into the tree.)
    juce::ValueTree currentFullState();

    // Pristine post-construction state; factory presets build on copies of it.
    const juce::ValueTree& defaultState() const noexcept { return defaultState_; }

    // Apply a full rig state immediately (host setState path).
    void applyState(juce::ValueTree v);

    // Click-free switch: fade the output, apply on the message thread once
    // silent, fade back. Used by the preset bar / factory presets.
    void requestStateLoad(juce::ValueTree v);

    uint64_t unitSerial() const noexcept { return rack_.tolerances().serial(); }

    // Saved-state format stamp (DAW blobs + .s42n). Loading is tolerant of
    // older/absent stamps (missing params -> defaults, unknown params ->
    // dropped); bump only for changes that need an explicit migration.
    static constexpr int kStateVersion = 1;

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }
    s42::Rack& rack() noexcept { return rack_; }
    solar::PatchBay& patchBay() noexcept { return patchBay_; }
    solar::KeyboardState& keyboardState() noexcept { return kbState_; }
    solar::KbTouchState& kbTouch() noexcept { return kbTouch_; }
    solar::PresetManager& presets() noexcept { return *presetManager_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    s42::Rack::Controls controlsFromParams() const noexcept;
    float param(const char* id) const noexcept;

    void writeSessionChildren(juce::ValueTree& tree) const; // CARTRIDGES + TOLERANCES
    void applyUnitSerial(uint64_t serial);                  // suspend + reseed + re-prepare
    juce::ValueTree mergedWithDefaults(const juce::ValueTree& v) const;

    static constexpr uint32_t packSlot(int cart, int prog) noexcept
    {
        return (uint32_t) ((cart & 0xF) << 4 | (prog & 0xF));
    }
    static constexpr int slotCart(uint32_t s) noexcept { return (int) (s >> 4) & 0xF; }
    static constexpr int slotProg(uint32_t s) noexcept { return (int) s & 0xF; }

    s42::Rack rack_;
    juce::AudioProcessorValueTreeState apvts_;
    solar::PatchBay patchBay_ { apvts_.state, rack_ };

    // Keyboard setup: message thread edits ride the KEYBOARD state tree and
    // land here through a seqlock; gestures are plain atomics.
    s42::SeqlockBox<s42::KbConfig> kbShare_;
    solar::KbTouchState kbTouch_;

    // MIDI-in (M9a): the performer lives on the audio thread; the timer is
    // the message-thread half of latch mode, turning toggle-counter deltas
    // into dN.hold parameter flips (panel HOLD and MIDI share one state).
    void timerCallback() override;
    s42::MidiPerformer midiPerf_;
    uint32_t midiTogglesSeen_[6] = {};
    solar::KeyboardState kbState_ { apvts_.state,
                                    [this](const s42::KbConfig& c) { kbShare_.write(c); } };
    mutable s42::KbConfig kbConfigCache_ {};

    // Cartridge slot contents (M7): what each FV-1 chip currently HOLDS —
    // distinct from the inserted-cartridge/toggle params. The audio thread
    // detects 1-2-3 flips and re-latches from the inserted cartridge;
    // applyState() restores a saved pair (which may differ from inserted).
    mutable std::atomic<int> lastProgL_ { 0 }, lastProgR_ { 0 };
    mutable std::atomic<uint32_t> loadedL_ { packSlot(0, 0) }, loadedR_ { packSlot(0, 0) };

    // Click-free state swaps: short output fade around a preset load. The
    // audio thread mirrors its gain so the message thread can wait for the
    // fade to actually land before applying (and not wait forever when the
    // transport is stopped).
    void scheduleApply(int token, int attempt);
    std::atomic<float> fadeTarget_ { 1.0f };
    std::atomic<float> fadeGainShared_ { 1.0f };
    std::atomic<uint64_t> blocksProcessed_ { 0 };
    float fadeGain_ = 1.0f, fadeStep_ = 0.001f;
    juce::ValueTree pendingLoad_;
    int loadToken_ = 0;
    uint64_t blocksAtLoadRequest_ = 0;
    double loadRequestMs_ = 0.0;

    double lastSampleRate_ = 0.0;
    int lastBlockSize_ = 0;

    juce::ValueTree defaultState_;
    std::unique_ptr<solar::PresetManager> presetManager_;

    JUCE_DECLARE_WEAK_REFERENCEABLE(Solar42NProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Solar42NProcessor)
};
