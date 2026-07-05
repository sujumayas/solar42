#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/TuningConstants.h"
#include "state/PresetManager.h"

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
    defaultState_ = apvts_.copyState(); // pristine tree; factory presets build on copies
    presetManager_ = std::make_unique<solar::PresetManager>(*this);
}

Solar42NProcessor::~Solar42NProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout Solar42NProcessor::createLayout()
{
    namespace tn = s42::tuning;
    using P = juce::AudioParameterFloat;
    using B = juce::AudioParameterBool;
    using C = juce::AudioParameterChoice;
    using Group = juce::AudioProcessorParameterGroup;
    using Attr = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto range01 = juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f);
    const auto bipolar = juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0f);

    // ---- M7 naming pass: automation lanes and knob bubbles show real units.
    // Exponential maps mirror controlsFromParams(); IDs/versionHints untouched.
    auto secText = [](float sec)
    {
        return sec < 0.0995f ? juce::String(sec * 1000.0f, 0) + " ms"
             : sec < 9.95f   ? juce::String(sec, 2) + " s"
                             : juce::String(sec, 1) + " s";
    };
    auto hzText = [](float hz)
    {
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, 2) + " kHz"
             : hz >= 10.0f   ? juce::String(hz, 1) + " Hz"
                             : juce::String(hz, 2) + " Hz";
    };
    auto expSec = [secText](float lo, float hi)
    {
        return Attr().withStringFromValueFunction(
            [=](float v, int) { return secText(expMap(v, lo, hi)); });
    };
    auto expHz = [hzText](float lo, float hi)
    {
        return Attr().withStringFromValueFunction(
            [=](float v, int) { return hzText(expMap(v, lo, hi)); });
    };
    auto pct = []
    {
        return Attr().withStringFromValueFunction(
            [](float v, int) { return juce::String(v * 100.0f, 0) + " %"; });
    };
    auto taperDb = [] // squared (audio-taper) volumes -> dB
    {
        return Attr().withStringFromValueFunction(
            [](float v, int)
            {
                return v <= 0.001f ? juce::String("-inf dB")
                                   : juce::String(40.0f * std::log10(v), 1) + " dB";
            });
    };
    auto panText = []
    {
        return Attr().withStringFromValueFunction(
            [](float v, int)
            {
                const int p = (int) std::lround((v - 0.5f) * 200.0f);
                return p == 0 ? juce::String("centre")
                     : p < 0  ? "L " + juce::String(-p) + " %"
                              : "R " + juce::String(p) + " %";
            });
    };
    auto volts = [](float scale)
    {
        return Attr().withStringFromValueFunction(
            [scale](float v, int) { return juce::String(v * scale, 2) + " V"; });
    };
    auto semis = [](float span) // symmetric around centre
    {
        return Attr().withStringFromValueFunction(
            [span](float v, int)
            { return juce::String((v - 0.5f) * span, 1) + " st"; });
    };

    // ---- Classic drone voices (DRONE 1, 2, 4, 5).
    const float tuneDefaults[] = { 0.45f, 0.52f, 0.30f, 0.50f, 0.50f };
    for (int v = 0; v < s42::Rack::kClassicVoices; ++v)
    {
        const juce::String id(kDroneIds[v]);
        const juce::String name = "Drone " + id.substring(1);
        auto grp = std::make_unique<Group>(id, name, " | ");
        for (int g = 0; g < s42::ClassicDroneVoice::kNumGens; ++g)
        {
            const auto n = juce::String(g + 1);
            grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".gen" + n + ".tune", 1),
                                              name + " · Gen " + n + " Tune", range01,
                                              tuneDefaults[g], pct()));
            grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".gen" + n + ".mute", 1),
                                              name + " · Gen " + n + " Mute", g >= 3));
            grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".gen" + n + ".mod", 1),
                                              name + " · Gen " + n + " Mod", false));
        }
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".volt", 1), name + " · Volt",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".att", 1), name + " · Attack",
                                          range01, 0.45f,
                                          expSec(tn::kDroneAttMin, tn::kDroneAttMax)));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".rls", 1), name + " · Release",
                                          range01, 0.6f,
                                          expSec(tn::kDroneRlsMin, tn::kDroneRlsMax)));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".hold", 1), name + " · Hold", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".gate", 1),
                                          name + " · Gate (Keypad " + id.substring(1) + ")", false));
        layout.add(std::move(grp));
    }

    // ---- Papa Srapa voices (DRONE 3, 6). Rate/pitch laws live inside the
    // voice model, so those lanes stay honest percentages.
    for (int s = 0; s < s42::Rack::kSrapaVoices; ++s)
    {
        const juce::String id(kSrapaIds[s]);
        const juce::String name = "Drone " + id.substring(1);
        auto grp = std::make_unique<Group>(id, name + " (Papa Srapa)", " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".rate", 1), name + " · Rate",
                                          range01, 0.3f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".fmamt", 1), name + " · FM Amount",
                                          range01, 0.5f, pct()));
        grp->addChild(std::make_unique<C>(juce::ParameterID(id + ".divider", 1), name + " · Divider",
                                          juce::StringArray { "1", "2", "4", "8", "16" }, 0));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".pitch", 1), name + " · Pitch",
                                          range01, 0.5f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".noise", 1), name + " · Noise",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".fm", 1), name + " · FM", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".am", 1), name + " · AM", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".x10", 1), name + " · Rate x10", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".hi", 1), name + " · Pitch Hi", false));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".att", 1), name + " · Attack",
                                          range01, 0.3f,
                                          expSec(tn::kDroneAttMin, tn::kDroneAttMax)));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".rls", 1), name + " · Release",
                                          range01, 0.5f,
                                          expSec(tn::kDroneRlsMin, tn::kDroneRlsMax)));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".hold", 1), name + " · Hold", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".gate", 1),
                                          name + " · Gate (Keypad " + id.substring(1) + ")", false));
        layout.add(std::move(grp));
    }

    // ---- VCO A/B + Envelope A/B.
    for (const char* vid : { "vcoA", "vcoB" })
    {
        const juce::String id(vid);
        const juce::String name = id == "vcoA" ? "VCO A" : "VCO B";
        auto grp = std::make_unique<Group>(id, name, " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".cvamt", 1), name + " · CV Amt",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".tune", 1), name + " · Tune",
                                          range01, 0.5f, semis(12.0f)));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".wave", 1), name + " · Wave",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".pwm", 1), name + " · PWM Amt",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".pw", 1), name + " · PW",
                                          range01, 0.5f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".oct", 1), name + " · Oct +3", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".sub", 1), name + " · Sub -1", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".exp", 1), name + " · Exp CV", false));
        layout.add(std::move(grp));
    }
    for (const char* eid : { "envA", "envB" })
    {
        const juce::String id(eid);
        const juce::String name = id == "envA" ? "Envelope A" : "Envelope B";
        auto grp = std::make_unique<Group>(id, name, " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".a", 1), name + " · Attack",
                                          range01, 0.1f, expSec(tn::kVcoAttMin, tn::kVcoAttMax)));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".d", 1), name + " · Decay",
                                          range01, 0.35f, expSec(tn::kVcoDecMin, tn::kVcoDecMax)));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".s", 1), name + " · Sustain",
                                          range01, 0.7f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".r", 1), name + " · Release",
                                          range01, 0.3f, expSec(tn::kVcoRelMin, tn::kVcoRelMax)));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".hold", 1), name + " · Hold", false));
        grp->addChild(std::make_unique<B>(juce::ParameterID(id + ".loop", 1), name + " · Loop", false));
        layout.add(std::move(grp));
    }

    // ---- 10-channel mixer (PAN = filter routing).
    {
        auto grp = std::make_unique<Group>("mix", "Voice Mixer", " | ");
        for (int c = 0; c < s42::MixerModule::kChannels; ++c)
        {
            const juce::String id = "mix." + juce::String(kMixChIds[c]);
            const juce::String name = "Mix · " + juce::String(kMixChNames[c]);
            grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".vol", 1), name + " Vol",
                                              range01, 0.7f, taperDb()));
            grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".pan", 1), name + " Pan",
                                              range01, 0.5f, panText()));
        }
        layout.add(std::move(grp));
    }

    // ---- Dual Polivoks filter + shared double distortion.
    {
        auto grp = std::make_unique<Group>("filt", "Filter", " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.freqL", 1), "Filter L · Freq",
                                          range01, 0.55f,
                                          expHz(tn::kFilterFreqMin, tn::kFilterFreqMax)));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.resL", 1), "Filter L · Res",
                                          range01, 0.30f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID("filt.bpL", 1), "Filter L · BP Mode", false));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.freqR", 1), "Filter R · Freq",
                                          range01, 0.55f,
                                          expHz(tn::kFilterFreqMin, tn::kFilterFreqMax)));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.resR", 1), "Filter R · Res",
                                          range01, 0.30f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID("filt.bpR", 1), "Filter R · BP Mode", false));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.modL", 1), "Filter · Mod L",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.modR", 1), "Filter · Mod R",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID("filt.link", 1), "Filter · Link", false));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.dist", 1), "Filter · Dist",
                                          range01, 0.0f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID("filt.gain", 1), "Filter · Gain",
                                          range01, 0.3f, pct()));
        layout.add(std::move(grp));
    }

    // ---- Dual FV-1 effector (cartridge slot + shared X/Y/Z/BLEND).
    {
        auto grp = std::make_unique<Group>("fx", "Dual Effector", " | ");
        grp->addChild(std::make_unique<C>(juce::ParameterID("fx.cart", 1), "FX · Cartridge",
                                          juce::StringArray { "CATHEDRAL", "TIME", "VIBROTREM", "OCHRE" }, 0));
        grp->addChild(std::make_unique<C>(juce::ParameterID("fx.progL", 1), "FX · Program L",
                                          juce::StringArray { "1", "2", "3" }, 0));
        grp->addChild(std::make_unique<C>(juce::ParameterID("fx.progR", 1), "FX · Program R",
                                          juce::StringArray { "1", "2", "3" }, 0));
        grp->addChild(std::make_unique<P>(juce::ParameterID("fx.x", 1), "FX · X", range01, 0.5f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID("fx.y", 1), "FX · Y", range01, 0.5f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID("fx.z", 1), "FX · Z", range01, 0.5f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID("fx.blend", 1), "FX · Blend",
                                          range01, 0.35f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID("fx.potq", 1), "FX · 9-bit Pots", true));
        layout.add(std::move(grp));
    }

    // ---- Bottom modulation strip (manual pp10-12).
    const char* lfoIds[] = { "lfoA", "lfoB" };
    const char* lfoNames[] = { "LFO A", "LFO B" };
    const float lfoMins[] = { tn::kLfoAMin, tn::kLfoBMin };
    const float lfoMaxs[] = { tn::kLfoAMax, tn::kLfoBMax };
    for (int li = 0; li < 2; ++li)
    {
        const juce::String id(lfoIds[li]), name(lfoNames[li]);
        auto grp = std::make_unique<Group>(id, name, " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".wave", 1),
                                          name + " · Wave", range01, 0.5f, pct()));
        grp->addChild(std::make_unique<P>(juce::ParameterID(id + ".rate", 1),
                                          name + " · Rate", range01, 0.4f,
                                          expHz(lfoMins[li], lfoMaxs[li])));
        grp->addChild(std::make_unique<C>(juce::ParameterID(id + ".range", 1), name + " · Range",
                                          juce::StringArray { "x1", "x6", "x10" }, 0));
        layout.add(std::move(grp));
    }
    {
        auto grp = std::make_unique<Group>("joy", "Joystick", " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID("joy.x", 1), "Joystick · X",
                                          bipolar, 0.0f, volts(10.0f)));
        grp->addChild(std::make_unique<P>(juce::ParameterID("joy.y", 1), "Joystick · Y",
                                          bipolar, 0.0f, volts(10.0f)));
        grp->addChild(std::make_unique<P>(juce::ParameterID("joy.xoff", 1), "Joystick · X Offset",
                                          bipolar, 0.0f, volts(5.0f)));
        grp->addChild(std::make_unique<P>(juce::ParameterID("joy.yoff", 1), "Joystick · Y Offset",
                                          bipolar, 0.0f, volts(5.0f)));
        layout.add(std::move(grp));
    }
    {
        auto grp = std::make_unique<Group>("seq", "Step Sequencer", " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID("seq.pulser", 1), "Seq · Pulser Rate",
                                          range01, 0.4f, expHz(tn::kPulserMin, tn::kPulserMax)));
        grp->addChild(std::make_unique<C>(juce::ParameterID("seq.stages", 1), "Seq · Stages",
                                          juce::StringArray { "3", "4", "5" }, 2));
        for (int s = 1; s <= 5; ++s)
        {
            grp->addChild(std::make_unique<P>(juce::ParameterID("seq.step" + juce::String(s), 1),
                                              "Seq · Step " + juce::String(s), range01, 0.0f,
                                              volts(5.0f)));
            grp->addChild(std::make_unique<B>(juce::ParameterID("seq.gate" + juce::String(s), 1),
                                              "Seq · Gate " + juce::String(s), true));
        }
        layout.add(std::move(grp));
    }
    {
        auto grp = std::make_unique<Group>("pre", "Preamp & Follower", " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID("pre.gain", 1), "Preamp · Gain",
                                          range01, 0.25f,
                                          Attr().withStringFromValueFunction(
                                              [](float v, int)
                                              { return juce::String(40.0f * v, 1) + " dB"; })));
        grp->addChild(std::make_unique<P>(juce::ParameterID("envf.att", 1), "Env Follower · Attack",
                                          range01, 0.2f, expSec(0.001f, 0.5f)));
        grp->addChild(std::make_unique<P>(juce::ParameterID("envf.rel", 1), "Env Follower · Release",
                                          range01, 0.4f, expSec(0.01f, 2.0f)));
        layout.add(std::move(grp));
    }

    // ---- Room light (photo-sensor environment) + master.
    {
        auto grp = std::make_unique<Group>("global", "Room & Master", " | ");
        grp->addChild(std::make_unique<P>(juce::ParameterID("room.light", 1), "Room · Light",
                                          range01, 0.35f, pct()));
        grp->addChild(std::make_unique<B>(juce::ParameterID("room.flicker", 1), "Room · Mains Flicker", false));
        grp->addChild(std::make_unique<P>(juce::ParameterID("master.vol", 1), "Master · Volume",
                                          range01, 0.70f, taperDb()));
        layout.add(std::move(grp));
    }

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

    // Cartridge slots: a 1-2-3 flip (knob, automation, restored param — any
    // path) latches that program from the currently-inserted cartridge; the
    // loaded pair is separate state so a cartridge swap alone changes nothing.
    const int cart = (int) param("fx.cart") % 4;
    const int progL = (int) param("fx.progL") % 3;
    const int progR = (int) param("fx.progR") % 3;
    if (progL != lastProgL_.load(std::memory_order_relaxed))
    {
        lastProgL_.store(progL, std::memory_order_relaxed);
        loadedL_.store(packSlot(cart, progL), std::memory_order_relaxed);
    }
    if (progR != lastProgR_.load(std::memory_order_relaxed))
    {
        lastProgR_.store(progR, std::memory_order_relaxed);
        loadedR_.store(packSlot(cart, progR), std::memory_order_relaxed);
    }
    const uint32_t slotL = loadedL_.load(std::memory_order_relaxed);
    const uint32_t slotR = loadedR_.load(std::memory_order_relaxed);
    c.fx.cartridge = cart;
    c.fx.progL = progL;
    c.fx.progR = progR;
    c.fx.loadedCartL = slotCart(slotL);
    c.fx.loadedProgL = slotProg(slotL);
    c.fx.loadedCartR = slotCart(slotR);
    c.fx.loadedProgR = slotProg(slotR);
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
    lastSampleRate_ = sampleRate;
    lastBlockSize_ = samplesPerBlock;
    fadeStep_ = 1.0f / (float) (0.012 * sampleRate); // ~12 ms state-swap fade
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

    // Click-free state swaps: short linear fade around a preset load. Idle
    // cost is one atomic load per block.
    const float target = fadeTarget_.load(std::memory_order_relaxed);
    if (fadeGain_ < 1.0f || target < 1.0f)
    {
        float* l = buffer.getWritePointer(0);
        float* r = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
        {
            fadeGain_ += juce::jlimit(-fadeStep_, fadeStep_, target - fadeGain_);
            l[i] *= fadeGain_;
            r[i] *= fadeGain_;
        }
        fadeGainShared_.store(fadeGain_, std::memory_order_relaxed);
    }
    blocksProcessed_.fetch_add(1, std::memory_order_relaxed);

    for (int ch = 2; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

juce::AudioProcessorEditor* Solar42NProcessor::createEditor()
{
    return new Solar42NEditor(*this);
}

void Solar42NProcessor::writeSessionChildren(juce::ValueTree& tree) const
{
    // What each FV-1 chip holds right now (may differ from inserted+toggles).
    auto carts = tree.getOrCreateChildWithName("CARTRIDGES", nullptr);
    const uint32_t slotL = loadedL_.load(std::memory_order_relaxed);
    const uint32_t slotR = loadedR_.load(std::memory_order_relaxed);
    carts.setProperty("cartL", slotCart(slotL), nullptr);
    carts.setProperty("progL", slotProg(slotL), nullptr);
    carts.setProperty("cartR", slotCart(slotR), nullptr);
    carts.setProperty("progR", slotProg(slotR), nullptr);

    // The unit serial: a patch always sounds like *your* unit.
    auto tol = tree.getOrCreateChildWithName("TOLERANCES", nullptr);
    tol.setProperty("serial",
                    juce::String::toHexString((juce::int64) rack_.tolerances().serial()),
                    nullptr);
}

juce::ValueTree Solar42NProcessor::currentFullState()
{
    auto tree = apvts_.copyState(); // deep copy incl. CABLES / KEYBOARD / UI
    writeSessionChildren(tree);
    return tree;
}

void Solar42NProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = currentFullState().createXml())
        copyXmlToBinary(*xml, destData);
}

void Solar42NProcessor::applyState(juce::ValueTree v)
{
    if (!v.isValid() || !v.hasType(apvts_.state.getType()))
        return;

    apvts_.replaceState(v);

    // Cartridge slots: prefer the explicit child (M7 states); pre-M7 states
    // derive loaded = inserted + toggles. Order matters: trackers first so a
    // racing controlsFromParams() flip is corrected by the second store —
    // worst case is one sub-block of the derived program.
    const int cart = (int) param("fx.cart") % 4;
    const int progL = (int) param("fx.progL") % 3;
    const int progR = (int) param("fx.progR") % 3;
    lastProgL_.store(progL, std::memory_order_relaxed);
    lastProgR_.store(progR, std::memory_order_relaxed);
    const auto carts = apvts_.state.getChildWithName("CARTRIDGES");
    loadedL_.store(packSlot((int) carts.getProperty("cartL", cart),
                            (int) carts.getProperty("progL", progL)),
                   std::memory_order_relaxed);
    loadedR_.store(packSlot((int) carts.getProperty("cartR", cart),
                            (int) carts.getProperty("progR", progR)),
                   std::memory_order_relaxed);

    // Unit serial (missing child = keep the current unit).
    const auto tol = apvts_.state.getChildWithName("TOLERANCES");
    const auto serialHex = tol.getProperty("serial").toString();
    if (serialHex.isNotEmpty())
        applyUnitSerial((uint64_t) serialHex.getHexValue64());

    patchBay_.rebind();
    kbState_.rebind();
}

void Solar42NProcessor::applyUnitSerial(uint64_t serial)
{
    if (serial == rack_.tolerances().serial())
        return;

    // "Swap hardware units": tolerances are baked in at prepare(), so a new
    // serial re-prepares the rack under a processing suspension. Rare, and
    // preset loads arrive faded-out anyway.
    suspendProcessing(true);
    rack_.tolerances().reseed(serial);
    if (lastSampleRate_ > 0.0)
        rack_.prepare(lastSampleRate_, lastBlockSize_);
    suspendProcessing(false);
}

void Solar42NProcessor::requestStateLoad(juce::ValueTree v)
{
    // Fade out (~12 ms), apply on the message thread once the fade has
    // actually landed, fade back in. A newer request supersedes a pending
    // one via the token.
    pendingLoad_ = v;
    const int token = ++loadToken_;
    blocksAtLoadRequest_ = blocksProcessed_.load(std::memory_order_relaxed);
    loadRequestMs_ = juce::Time::getMillisecondCounterHiRes();
    fadeTarget_.store(0.0f, std::memory_order_relaxed);
    scheduleApply(token, 0);
}

void Solar42NProcessor::scheduleApply(int token, int attempt)
{
    // Timer wall-clock is advisory only (a starved timer thread can fire
    // early); the apply decision is made from measured state: the fade has
    // actually landed, or the transport is provably idle (no callbacks, so
    // nobody hears the swap), or a hard cap expired.
    juce::Timer::callAfterDelay(15,
        [wr = juce::WeakReference<Solar42NProcessor>(this), token, attempt]
        {
            auto* p = wr.get();
            if (p == nullptr || token != p->loadToken_)
                return;
            const double elapsed =
                juce::Time::getMillisecondCounterHiRes() - p->loadRequestMs_;
            const bool audioRunning =
                p->blocksProcessed_.load(std::memory_order_relaxed) != p->blocksAtLoadRequest_;
            const bool faded =
                p->fadeGainShared_.load(std::memory_order_relaxed) <= 0.02f;
            if (!(faded || (!audioRunning && elapsed >= 30.0) || elapsed >= 250.0))
            {
                p->scheduleApply(token, attempt + 1);
                return;
            }
            p->applyState(p->pendingLoad_);
            p->pendingLoad_ = juce::ValueTree();
            p->fadeTarget_.store(1.0f, std::memory_order_relaxed);
        });
}

void Solar42NProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        applyState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Solar42NProcessor();
}
