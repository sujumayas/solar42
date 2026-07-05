#pragma once

#include <atomic>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/keyboard/KbConfig.h"

namespace solar {

// UI -> audio-thread performance gestures (plate pressures + transpose
// buttons). The plate components store atomics; controlsFromParams()
// snapshots them into Rack::Controls every block.
struct KbTouchState
{
    std::atomic<float> plate[12] {};
    std::atomic<bool> button[2] { false, false };

    s42::KbTouch snapshot() const noexcept
    {
        s42::KbTouch t;
        for (int i = 0; i < 12; ++i)
            t.plate[i] = plate[i].load(std::memory_order_relaxed);
        t.button[0] = button[0].load(std::memory_order_relaxed);
        t.button[1] = button[1].load(std::memory_order_relaxed);
        return t;
    }
};

// KbConfig <-> ValueTree encoding. The KEYBOARD child of the APVTS tree is
// the persistent home of the full keyboard setup (plan §state: config incl.
// microtuning + presets A-D); presets are four child trees of the same shape.
namespace kbio {

inline juce::String joinFloats(const float* v, int n)
{
    juce::StringArray a;
    for (int i = 0; i < n; ++i)
        a.add(juce::String(v[i], 5));
    return a.joinIntoString(" ");
}

inline void splitFloats(const juce::String& s, float* v, int n)
{
    const auto tok = juce::StringArray::fromTokens(s, " ", "");
    for (int i = 0; i < n && i < tok.size(); ++i)
        v[i] = tok[i].getFloatValue();
}

inline void encodeSide(juce::ValueTree t, const s42::KbSideConfig& s)
{
    t.setProperty("mode", (int) s.mode, nullptr);
    t.setProperty("arpHold", s.arp.hold, nullptr);
    t.setProperty("arpClock", s.arp.clockRatio, nullptr);
    t.setProperty("arpDir", (int) s.arp.direction, nullptr);
    t.setProperty("arpVar", s.arp.variation, nullptr);
    t.setProperty("arpInterval", s.arp.intervalSemis, nullptr);
    t.setProperty("arpRhythm", (int) s.arp.rhythmMask, nullptr);
    t.setProperty("arpRhythmLen", s.arp.rhythmLen, nullptr);
    t.setProperty("seqRun", s.seq.keyRun, nullptr);
    t.setProperty("seqLen", s.seq.length, nullptr);
    t.setProperty("seqClock", s.seq.clockRatio, nullptr);
    t.setProperty("seqDir", (int) s.seq.direction, nullptr);
    t.setProperty("seqGatedCv", s.seq.gatedCv, nullptr);
    t.setProperty("seqRhythm", (int) s.seq.rhythmMask, nullptr);
    t.setProperty("seqRhythmLen", s.seq.rhythmLen, nullptr);
    t.setProperty("seqValues", joinFloats(s.seq.value, 16), nullptr);
    juce::String gates;
    for (int i = 0; i < 16; ++i)
        gates << (s.seq.gate[i] ? "1" : "0");
    t.setProperty("seqGates", gates, nullptr);
}

inline void decodeSide(const juce::ValueTree& t, s42::KbSideConfig& s)
{
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    s.mode = (s42::KbMode) clampi((int) t.getProperty("mode", 0), 0, 2);
    s.arp.hold = t.getProperty("arpHold", false);
    s.arp.clockRatio = clampi(t.getProperty("arpClock", s42::kKbClockUnity), 0, s42::kKbNumClockRatios - 1);
    s.arp.direction = (s42::KbDirection) clampi((int) t.getProperty("arpDir", 0), 0, 3);
    s.arp.variation = clampi(t.getProperty("arpVar", 0), 0, 3);
    s.arp.intervalSemis = clampi(t.getProperty("arpInterval", 12), 1, 12);
    s.arp.rhythmMask = (uint8_t) (int) t.getProperty("arpRhythm", 1);
    s.arp.rhythmLen = clampi(t.getProperty("arpRhythmLen", 1), 1, 8);
    s.seq.keyRun = t.getProperty("seqRun", false);
    s.seq.length = clampi(t.getProperty("seqLen", 8), 2, 16);
    s.seq.clockRatio = clampi(t.getProperty("seqClock", s42::kKbClockUnity), 0, s42::kKbNumClockRatios - 1);
    s.seq.direction = (s42::KbDirection) clampi((int) t.getProperty("seqDir", 0), 0, 3);
    s.seq.gatedCv = t.getProperty("seqGatedCv", false);
    s.seq.rhythmMask = (uint8_t) (int) t.getProperty("seqRhythm", 1);
    s.seq.rhythmLen = clampi(t.getProperty("seqRhythmLen", 1), 1, 8);
    splitFloats(t.getProperty("seqValues", "").toString(), s.seq.value, 16);
    const auto gates = t.getProperty("seqGates", "").toString();
    for (int i = 0; i < 16 && i < gates.length(); ++i)
        s.seq.gate[i] = gates[i] == '1';
}

inline void encode(juce::ValueTree t, const s42::KbConfig& c)
{
    t.setProperty("behaviour", (int) c.behaviour, nullptr);
    t.setProperty("scaleMask", (int) c.scaleMask, nullptr);
    t.setProperty("root", c.rootNote, nullptr);
    t.setProperty("plates", joinFloats(c.plateVolts, 12), nullptr);
    t.setProperty("offsets", joinFloats(c.offsetVolts, 2), nullptr);
    t.setProperty("portamento", c.portamento, nullptr);
    t.setProperty("legato", c.legato, nullptr);
    t.setProperty("vibSpeed", c.vibSpeed, nullptr);
    t.setProperty("vibDepth", c.vibDepth, nullptr);
    t.setProperty("vibDelay", c.vibDelay, nullptr);
    t.setProperty("vibPress", c.vibPressure, nullptr);
    t.setProperty("pressMode", (int) c.pressureMode, nullptr);
    t.setProperty("rise", c.rise, nullptr);
    t.setProperty("fall", c.fall, nullptr);
    t.setProperty("bpm", c.bpm, nullptr);
    t.setProperty("bpmEdit", (int) c.bpmEdit, nullptr);
    for (int s = 0; s < 2; ++s)
        encodeSide(t.getOrCreateChildWithName(s == 0 ? "SIDE0" : "SIDE1", nullptr), c.side[s]);
}

inline s42::KbConfig decode(const juce::ValueTree& t)
{
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    s42::KbConfig c;
    c.behaviour = (s42::KbBehaviour) clampi((int) t.getProperty("behaviour", 0), 0, 2);
    c.scaleMask = (uint16_t) ((int) t.getProperty("scaleMask", 0x0FFF) & 0x0FFF);
    c.rootNote = clampi(t.getProperty("root", 0), 0, 11);
    splitFloats(t.getProperty("plates", "").toString(), c.plateVolts, 12);
    splitFloats(t.getProperty("offsets", "").toString(), c.offsetVolts, 2);
    c.portamento = clampi(t.getProperty("portamento", 0), 0, 255);
    c.legato = t.getProperty("legato", false);
    c.vibSpeed = clampi(t.getProperty("vibSpeed", 32), 0, 127);
    c.vibDepth = clampi(t.getProperty("vibDepth", 0), 0, 127);
    c.vibDelay = clampi(t.getProperty("vibDelay", 0), 0, 127);
    c.vibPressure = t.getProperty("vibPress", false);
    c.pressureMode = (s42::KbPressureMode) clampi((int) t.getProperty("pressMode", 0), 0, 4);
    c.rise = clampi(t.getProperty("rise", 0), 0, 255);
    c.fall = clampi(t.getProperty("fall", 0), 0, 255);
    c.bpm = (float) juce::jlimit(10.0, 300.0, (double) t.getProperty("bpm", 120.0f));
    c.bpmEdit = (uint32_t) (int) t.getProperty("bpmEdit", 0);
    decodeSide(t.getChildWithName("SIDE0"), c.side[0]);
    decodeSide(t.getChildWithName("SIDE1"), c.side[1]);
    return c;
}

} // namespace kbio

// Message-thread owner of the keyboard setup: binds the KEYBOARD child of
// the APVTS tree (so it persists with the plugin state), keeps a decoded
// cache for the UI, and pushes every change to the audio thread through the
// processor's SeqlockBox. UI components listen for change messages (same
// pattern as PatchBay) to refresh themselves.
class KeyboardState : public juce::ChangeBroadcaster
{
public:
    KeyboardState(juce::ValueTree& rootState, std::function<void(const s42::KbConfig&)> push)
        : root_(rootState), push_(std::move(push))
    {
        rebind();
    }

    // Call after construction and after replaceState.
    void rebind()
    {
        tree_ = root_.getOrCreateChildWithName("KEYBOARD", nullptr);
        if (!tree_.hasProperty("bpm")) // fresh instance: write factory defaults
            kbio::encode(tree_, s42::KbConfig {});
        presets_ = tree_.getOrCreateChildWithName("PRESETS", nullptr);
        while (presets_.getNumChildren() < 4)
        {
            juce::ValueTree p("PRESET");
            kbio::encode(p, s42::KbConfig {});
            presets_.appendChild(p, nullptr);
        }
        cached_ = kbio::decode(tree_);
        if (push_)
            push_(cached_);
        sendChangeMessage();
    }

    s42::KbConfig config() const { return cached_; }

    void set(const s42::KbConfig& c)
    {
        cached_ = c;
        kbio::encode(tree_, c);
        if (push_)
            push_(c);
        sendChangeMessage();
    }

    s42::KbPresets presets() const
    {
        s42::KbPresets p;
        for (int i = 0; i < 4 && i < presets_.getNumChildren(); ++i)
            p.slot[i] = kbio::decode(presets_.getChild(i));
        return p;
    }

    void setPresets(const s42::KbPresets& p)
    {
        for (int i = 0; i < 4 && i < presets_.getNumChildren(); ++i)
            kbio::encode(presets_.getChild(i), p.slot[i]);
    }

private:
    juce::ValueTree& root_;
    juce::ValueTree tree_, presets_;
    s42::KbConfig cached_;
    std::function<void(const s42::KbConfig&)> push_;
};

} // namespace solar
