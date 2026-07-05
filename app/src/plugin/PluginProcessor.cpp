#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/TuningConstants.h"

namespace
{
float expMap(float v01, float lo, float hi) noexcept
{
    return lo * std::pow(hi / lo, v01);
}

// 42N voice / mixer-channel naming (07 §0-§1).
const char* const kDroneIds[] = { "d1", "d2", "d4", "d5" };
const char* const kSrapaIds[] = { "d3", "d6" };
const char* const kMixChIds[] = { "d1", "d2", "d3", "ext", "vcoA", "vcoB", "pre", "d4", "d5", "d6" };
const char* const kMixChNames[] = { "Drone 1", "Drone 2", "Drone 3", "Ext Audio",
                                    "VCO A", "VCO B", "Preamp", "Drone 4", "Drone 5", "Drone 6" };
} // namespace

Solar42NProcessor::Solar42NProcessor()
    : juce::AudioProcessor(BusesProperties()
          .withInput("Ext In", juce::AudioChannelSet::stereo(), true)   // EXT AUDIO / preamp source
          .withOutput("Wet Out", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "PARAMS", createLayout())
{
    patchBay_.rebind();
}

juce::AudioProcessorValueTreeState::ParameterLayout Solar42NProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto range01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f);
    const auto bipolar = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f);

    // ---- Classic drone voices (DRONE 1, 2, 4, 5).
    const float tuneDefaults[] = { 0.45f, 0.52f, 0.30f, 0.50f, 0.50f };
    for (int v = 0; v < s42::Rack::kClassicVoices; ++v)
    {
        const juce::String id(kDroneIds[v]);
        const juce::String name = "Drone " + id.substring(1);
        for (int g = 0; g < s42::ClassicDroneVoice::kNumGens; ++g)
        {
            const auto n = juce::String(g + 1);
            layout.add(std::make_unique<P>(juce::ParameterID(id + ".gen" + n + ".tune", 1),
                                           name + " · Gen " + n + " Tune", range01, tuneDefaults[g]));
            layout.add(std::make_unique<B>(juce::ParameterID(id + ".gen" + n + ".mute", 1),
                                           name + " · Gen " + n + " Mute", g >= 3));
            layout.add(std::make_unique<B>(juce::ParameterID(id + ".gen" + n + ".mod", 1),
                                           name + " · Gen " + n + " Mod", false));
        }
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".volt", 1), name + " · Volt", range01, 0.0f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".att", 1), name + " · Attack", range01, 0.45f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".rls", 1), name + " · Release", range01, 0.6f));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".hold", 1), name + " · Hold", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".gate", 1),
                                       name + " · Gate (Keypad " + id.substring(1) + ")", false));
    }

    // ---- Papa Srapa voices (DRONE 3, 6).
    for (int s = 0; s < s42::Rack::kSrapaVoices; ++s)
    {
        const juce::String id(kSrapaIds[s]);
        const juce::String name = "Drone " + id.substring(1);
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".rate", 1), name + " · Rate", range01, 0.3f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".fmamt", 1), name + " · FM Amount", range01, 0.5f));
        layout.add(std::make_unique<C>(juce::ParameterID(id + ".divider", 1), name + " · Divider",
                                       juce::StringArray { "1", "2", "4", "8", "16" }, 0));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".pitch", 1), name + " · Pitch", range01, 0.5f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".noise", 1), name + " · Noise", range01, 0.0f));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".fm", 1), name + " · FM", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".am", 1), name + " · AM", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".x10", 1), name + " · Rate x10", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".hi", 1), name + " · Pitch Hi", false));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".att", 1), name + " · Attack", range01, 0.3f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".rls", 1), name + " · Release", range01, 0.5f));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".hold", 1), name + " · Hold", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".gate", 1),
                                       name + " · Gate (Keypad " + id.substring(1) + ")", false));
    }

    // ---- VCO A/B + Envelope A/B.
    for (const char* vid : { "vcoA", "vcoB" })
    {
        const juce::String id(vid);
        const juce::String name = id == "vcoA" ? "VCO A" : "VCO B";
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".cvamt", 1), name + " · CV Amt", range01, 0.0f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".tune", 1), name + " · Tune", range01, 0.5f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".wave", 1), name + " · Wave", range01, 0.0f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".pwm", 1), name + " · PWM Amt", range01, 0.0f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".pw", 1), name + " · PW", range01, 0.5f));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".oct", 1), name + " · Oct +3", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".sub", 1), name + " · Sub -1", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".exp", 1), name + " · Exp CV", false));
    }
    for (const char* eid : { "envA", "envB" })
    {
        const juce::String id(eid);
        const juce::String name = id == "envA" ? "Envelope A" : "Envelope B";
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".a", 1), name + " · Attack", range01, 0.1f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".d", 1), name + " · Decay", range01, 0.35f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".s", 1), name + " · Sustain", range01, 0.7f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".r", 1), name + " · Release", range01, 0.3f));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".hold", 1), name + " · Hold", false));
        layout.add(std::make_unique<B>(juce::ParameterID(id + ".loop", 1), name + " · Loop", false));
    }

    // ---- 10-channel mixer (PAN = filter routing).
    for (int c = 0; c < s42::MixerModule::kChannels; ++c)
    {
        const juce::String id = "mix." + juce::String(kMixChIds[c]);
        const juce::String name = "Mix · " + juce::String(kMixChNames[c]);
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".vol", 1), name + " Vol", range01, 0.7f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".pan", 1), name + " Pan", range01, 0.5f));
    }

    // ---- Dual Polivoks filter + shared double distortion.
    layout.add(std::make_unique<P>(juce::ParameterID("filt.freqL", 1), "Filter L · Freq", range01, 0.55f));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.resL", 1), "Filter L · Res", range01, 0.30f));
    layout.add(std::make_unique<B>(juce::ParameterID("filt.bpL", 1), "Filter L · BP Mode", false));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.freqR", 1), "Filter R · Freq", range01, 0.55f));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.resR", 1), "Filter R · Res", range01, 0.30f));
    layout.add(std::make_unique<B>(juce::ParameterID("filt.bpR", 1), "Filter R · BP Mode", false));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.modL", 1), "Filter · Mod L", range01, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.modR", 1), "Filter · Mod R", range01, 0.0f));
    layout.add(std::make_unique<B>(juce::ParameterID("filt.link", 1), "Filter · Link", false));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.dist", 1), "Filter · Dist", range01, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("filt.gain", 1), "Filter · Gain", range01, 0.3f));

    // ---- Dual FV-1 effector (cartridge slot + shared X/Y/Z/BLEND).
    layout.add(std::make_unique<C>(juce::ParameterID("fx.cart", 1), "FX · Cartridge",
                                   juce::StringArray { "CATHEDRAL", "TIME", "VIBROTREM", "OCHRE" }, 0));
    layout.add(std::make_unique<C>(juce::ParameterID("fx.progL", 1), "FX · Program L",
                                   juce::StringArray { "1", "2", "3" }, 0));
    layout.add(std::make_unique<C>(juce::ParameterID("fx.progR", 1), "FX · Program R",
                                   juce::StringArray { "1", "2", "3" }, 0));
    layout.add(std::make_unique<P>(juce::ParameterID("fx.x", 1), "FX · X", range01, 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID("fx.y", 1), "FX · Y", range01, 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID("fx.z", 1), "FX · Z", range01, 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID("fx.blend", 1), "FX · Blend", range01, 0.35f));
    layout.add(std::make_unique<B>(juce::ParameterID("fx.potq", 1), "FX · 9-bit Pots", true));

    // ---- Bottom modulation strip (manual pp10-12).
    const char* lfoIds[] = { "lfoA", "lfoB" };
    const char* lfoNames[] = { "LFO A", "LFO B" };
    for (int li = 0; li < 2; ++li)
    {
        const juce::String id(lfoIds[li]), name(lfoNames[li]);
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".wave", 1),
                                       name + " · Wave", range01, 0.5f));
        layout.add(std::make_unique<P>(juce::ParameterID(id + ".rate", 1),
                                       name + " · Rate", range01, 0.4f));
        layout.add(std::make_unique<C>(juce::ParameterID(id + ".range", 1), name + " · Range",
                                       juce::StringArray { "x1", "x6", "x10" }, 0));
    }
    layout.add(std::make_unique<P>(juce::ParameterID("joy.x", 1), "Joystick · X", bipolar, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("joy.y", 1), "Joystick · Y", bipolar, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("joy.xoff", 1), "Joystick · X Offset", bipolar, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("joy.yoff", 1), "Joystick · Y Offset", bipolar, 0.0f));
    layout.add(std::make_unique<P>(juce::ParameterID("seq.pulser", 1), "Seq · Pulser Rate", range01, 0.4f));
    layout.add(std::make_unique<C>(juce::ParameterID("seq.stages", 1), "Seq · Stages",
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

    // ---- Room light (photo-sensor environment) + master.
    layout.add(std::make_unique<P>(juce::ParameterID("room.light", 1), "Room · Light", range01, 0.35f));
    layout.add(std::make_unique<B>(juce::ParameterID("room.flicker", 1), "Room · Mains Flicker", false));
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

    auto fparam = [this](const juce::String& id) { return param(id.toRawUTF8()); };
    auto bparam = [this](const juce::String& id) { return param(id.toRawUTF8()) > 0.5f; };

    for (int v = 0; v < s42::Rack::kClassicVoices; ++v)
    {
        const juce::String id(kDroneIds[v]);
        auto& d = c.drone[v];
        for (int g = 0; g < s42::ClassicDroneVoice::kNumGens; ++g)
        {
            const auto base = id + ".gen" + juce::String(g + 1);
            d.tune[g] = fparam(base + ".tune");
            d.mute[g] = bparam(base + ".mute");
            d.mod[g] = bparam(base + ".mod");
        }
        d.volt = fparam(id + ".volt");
        d.attackSec = expMap(fparam(id + ".att"), tn::kDroneAttMin, tn::kDroneAttMax);
        d.releaseSec = expMap(fparam(id + ".rls"), tn::kDroneRlsMin, tn::kDroneRlsMax);
        d.hold = bparam(id + ".hold");
        c.droneKey[v] = bparam(id + ".gate");
    }

    for (int s = 0; s < s42::Rack::kSrapaVoices; ++s)
    {
        const juce::String id(kSrapaIds[s]);
        auto& p = c.srapa[s];
        p.rate01 = fparam(id + ".rate");
        p.fmAmt = fparam(id + ".fmamt");
        p.dividerIdx = (int) fparam(id + ".divider") % 5;
        p.pitch01 = fparam(id + ".pitch");
        p.noise01 = fparam(id + ".noise");
        p.fmOn = bparam(id + ".fm");
        p.amOn = bparam(id + ".am");
        p.x10 = bparam(id + ".x10");
        p.hi = bparam(id + ".hi");
        p.attackSec = expMap(fparam(id + ".att"), tn::kDroneAttMin, tn::kDroneAttMax);
        p.releaseSec = expMap(fparam(id + ".rls"), tn::kDroneRlsMin, tn::kDroneRlsMax);
        p.hold = bparam(id + ".hold");
        c.srapaKey[s] = bparam(id + ".gate");
    }

    auto vcoFromParams = [&](const juce::String& id, s42::VcoVoice::Params& p)
    {
        p.cvAmt = fparam(id + ".cvamt");
        p.tune01 = fparam(id + ".tune");
        p.wave01 = fparam(id + ".wave");
        p.pwmAmt = fparam(id + ".pwm");
        p.pw01 = fparam(id + ".pw");
        p.octUp = bparam(id + ".oct");
        p.subOn = bparam(id + ".sub");
        p.expCv = bparam(id + ".exp");
    };
    vcoFromParams("vcoA", c.vcoA);
    vcoFromParams("vcoB", c.vcoB);

    auto envFromParams = [&](const juce::String& id, s42::EnvelopeModule::Params& p)
    {
        p.attackSec = expMap(fparam(id + ".a"), tn::kVcoAttMin, tn::kVcoAttMax);
        p.decaySec = expMap(fparam(id + ".d"), tn::kVcoDecMin, tn::kVcoDecMax);
        p.sustain = fparam(id + ".s");
        p.releaseSec = expMap(fparam(id + ".r"), tn::kVcoRelMin, tn::kVcoRelMax);
        p.hold = bparam(id + ".hold");
        p.loop = bparam(id + ".loop");
    };
    envFromParams("envA", c.envA);
    envFromParams("envB", c.envB);

    for (int ch = 0; ch < s42::MixerModule::kChannels; ++ch)
    {
        const juce::String id = "mix." + juce::String(kMixChIds[ch]);
        const float v = fparam(id + ".vol");
        c.mixer.vol[ch] = v * v; // audio taper
        c.mixer.pan[ch] = fparam(id + ".pan");
    }

    c.filter.freqHzL = expMap(fparam("filt.freqL"), tn::kFilterFreqMin, tn::kFilterFreqMax);
    c.filter.resL = fparam("filt.resL");
    c.filter.bpL = bparam("filt.bpL");
    c.filter.freqHzR = expMap(fparam("filt.freqR"), tn::kFilterFreqMin, tn::kFilterFreqMax);
    c.filter.resR = fparam("filt.resR");
    c.filter.bpR = bparam("filt.bpR");
    c.filter.modL = fparam("filt.modL");
    c.filter.modR = fparam("filt.modR");
    c.filter.link = bparam("filt.link");
    c.filter.dist = fparam("filt.dist");
    c.filter.gain = fparam("filt.gain");

    c.fx.cartridge = (int) param("fx.cart") % 4;
    c.fx.progL = (int) param("fx.progL") % 3;
    c.fx.progR = (int) param("fx.progR") % 3;
    c.fx.x = param("fx.x");
    c.fx.y = param("fx.y");
    c.fx.z = param("fx.z");
    c.fx.blend = param("fx.blend");
    c.fx.potQuantize = param("fx.potq") > 0.5f;

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

    kbShare_.read(kbConfigCache_); // keeps the last good copy on contention
    c.kb = kbConfigCache_;
    c.kbTouch = kbTouch_.snapshot();

    c.roomLight = param("room.light");
    c.mainsFlicker = param("room.flicker") > 0.5f;
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
    // Params + cables + keyboard (CABLES/KEYBOARD children ride in the APVTS
    // tree). M7 adds cartridges/unit-serial.
    if (auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void Solar42NProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
        {
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
            patchBay_.rebind();
            kbState_.rebind();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Solar42NProcessor();
}
