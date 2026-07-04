// solar42n_render — headless renderer. Full rig-state loading + golden
// regression arrive with the harness (M8/M9); until then it renders audition
// demos of whatever the engine can do so milestones can be heard.
//
// usage: solar42n_render [out.wav] [seconds]
#include "engine/Rack.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr double kSr = 48000.0;

void writeWav16(const std::string& path, const std::vector<float>& l,
                const std::vector<float>& r)
{
    const uint32_t frames = (uint32_t) l.size();
    const uint16_t channels = 2, bits = 16;
    const uint32_t sampleRate = (uint32_t) kSr;
    const uint32_t byteRate = sampleRate * channels * bits / 8;
    const uint32_t dataBytes = frames * channels * bits / 8;
    const uint16_t blockAlign = channels * bits / 8;

    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
    {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        std::exit(1);
    }
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };

    std::fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(channels);
    u32(sampleRate);
    u32(byteRate);
    u16(blockAlign);
    u16(bits);
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
    {
        for (const auto* ch : { &l, &r })
        {
            float v = (*ch)[i];
            v = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
            const auto s = (int16_t) (v * 32767.0f);
            std::fwrite(&s, 2, 1, f);
        }
    }
    std::fclose(f);
}
} // namespace

int main(int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : "solar42n-m3-fullpath.wav";
    const double seconds = argc > 2 ? std::atof(argv[2]) : 20.0;

    s42::Rack rack;
    rack.prepare(kSr, 512);

    // M3 audition scene — the full analog path in one take:
    //   DRONE 1  beating 4-gen cluster (M1's voice), panned toward filter L;
    //   DRONE 4  a slow second cluster on the right filter, LFO A on its
    //            sensor LED via the CV jack (gens 1-2 MOD down);
    //   DRONE 3  Papa Srapa: FM+AM siren-bird chirps, low in the mix;
    //   VCO A    sequenced by the 5-step sequencer (CV -> 1v/oct, gate ->
    //            Envelope A), saw, through the mixer center;
    //   VCO B    default-normalled to VCO A's osc (CV AMT up = 2-op FM pad,
    //            Envelope B HOLD open, low volume);
    //   Filters  L dark/resonant, R brighter + LFO B wobble on CV L (MOD L),
    //            DIST/GAIN warming everything.
    s42::Rack::Controls c;

    auto& d1 = c.drone[0];
    d1.tune[0] = 0.55f; d1.tune[1] = 0.32f; d1.tune[2] = 0.22f; d1.tune[3] = 0.35f;
    d1.mute[4] = true;
    d1.attackSec = 2.5f;
    d1.releaseSec = 6.0f;
    d1.hold = true;

    auto& d4 = c.drone[2]; // DRONE 4
    d4.tune[0] = 0.62f; d4.tune[1] = 0.60f; d4.tune[2] = 0.18f;
    d4.mute[3] = d4.mute[4] = true;
    d4.mod[0] = d4.mod[1] = true; // gens 1-2 follow the photo-sensor
    d4.attackSec = 4.0f;
    d4.releaseSec = 8.0f;
    d4.hold = true;

    auto& d3 = c.srapa[0]; // DRONE 3: slow chirps
    d3.rate01 = 0.45f;
    d3.fmAmt = 0.6f;
    d3.dividerIdx = 1;
    d3.pitch01 = 0.55f;
    d3.fmOn = true;
    d3.amOn = true;
    d3.attackSec = 1.5f;
    d3.releaseSec = 2.0f;
    d3.hold = true;

    c.vcoA.wave01 = 0.05f; // nearly pure saw
    c.vcoA.tune01 = 0.4f;
    c.envA.attackSec = 0.02f;
    c.envA.decaySec = 0.5f;
    c.envA.sustain = 0.25f;
    c.envA.releaseSec = 0.35f;

    c.vcoB.wave01 = 0.62f;  // sine/tri zone
    c.vcoB.cvAmt = 0.35f;   // eats VCO A through the normal = 2-op FM
    c.vcoB.tune01 = 0.15f;
    c.envB.hold = true;

    c.seq.pulserHz = 2.2f;
    c.seq.stages = 5;
    c.seq.cv[0] = 1.0f; c.seq.cv[1] = 1.583f; c.seq.cv[2] = 2.0f;
    c.seq.cv[3] = 1.417f; c.seq.cv[4] = 0.583f;
    c.seq.gateOn[3] = false; // a rest for articulation

    c.lfoA.rateHz = 0.15f; c.lfoA.wave = 1.0f;  // slow triangle -> sensor LED
    c.lfoB.rateHz = 0.28f; c.lfoB.wave = 0.9f;  // filter wobble

    auto& m = c.mixer;
    for (float& v : m.vol) v = 0.0f;
    m.vol[s42::MixerModule::ChD1] = 0.55f;   m.pan[s42::MixerModule::ChD1] = 0.25f;
    m.vol[s42::MixerModule::ChD4] = 0.5f;    m.pan[s42::MixerModule::ChD4] = 0.75f;
    m.vol[s42::MixerModule::ChD3] = 0.22f;   m.pan[s42::MixerModule::ChD3] = 0.65f;
    m.vol[s42::MixerModule::ChVcoA] = 0.42f; m.pan[s42::MixerModule::ChVcoA] = 0.45f;
    m.vol[s42::MixerModule::ChVcoB] = 0.25f; m.pan[s42::MixerModule::ChVcoB] = 0.6f;

    c.filter.freqHzL = 950.0f;  c.filter.resL = 0.55f;
    c.filter.freqHzR = 1600.0f; c.filter.resR = 0.45f;
    c.filter.modL = 0.25f;      // LFO B wobble (patched below)
    c.filter.dist = 0.35f;
    c.filter.gain = 0.45f;
    c.roomLight = 0.25f;
    c.masterVol = 0.6f;
    rack.setControls(c);

    // Patch cables: seq -> VCO A pitch + Envelope A gate; LFO A -> DRONE 4's
    // sensor LED; LFO B -> filter CV L (CV R follows the 42N normal).
    rack.requestPatch(s42::Inlet::VcoAVoctIn, s42::Outlet::SeqCvOut);
    rack.requestPatch(s42::Inlet::EnvAGateIn, s42::Outlet::SeqGateOut);
    rack.requestPatch(s42::Inlet::D4CvIn, s42::Outlet::LfoAOut);
    rack.requestPatch(s42::Inlet::FiltCvLIn, s42::Outlet::LfoBOut);

    const auto total = (size_t) (seconds * kSr);
    std::vector<float> l(total), r(total);

    const size_t dirtyAt = total * 3 / 4;
    for (size_t i = 0; i < total; i += 512)
    {
        if (i >= dirtyAt && d1.volt < 0.85f)
        {
            // creep DRONE 1's VOLT into the cross-FM dirty zone at the end
            d1.volt = 0.55f
                + 0.30f * (float) (i - dirtyAt) / (float) (total - dirtyAt);
            rack.setControls(c);
        }
        const auto n = std::min<size_t>(512, total - i);
        rack.process(l.data() + i, r.data() + i, nullptr, nullptr, (int) n);
    }

    writeWav16(out, l, r);
    std::printf("solar42n_render: wrote %s (%.1f s, M3 full path: drones + srapa + "
                "sequenced VCOs -> Polivoks + dist)\n",
                out.c_str(), seconds);
    return 0;
}
