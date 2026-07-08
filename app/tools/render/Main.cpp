// solar42n_render — headless renderer. Full rig-state loading + golden
// regression arrive with the harness (M8/M9); until then it renders audition
// demos of whatever the engine can do so milestones can be heard.
//
// usage: solar42n_render [out.wav] [seconds]
//        solar42n_render --bench [budgetPct]   (CPU gate, M9c P3)
#include "engine/Rack.h"

#include <chrono>
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
    // --bench [budgetPct]: CPU measurement instead of a WAV (M9c P3) —
    // realtime ratio over the same full demo rig; nonzero exit over budget.
    const bool bench = argc > 1 && std::strcmp(argv[1], "--bench") == 0;
    const double budgetPct = bench && argc > 2 ? std::atof(argv[2]) : 0.0;
    const std::string out = !bench && argc > 1 ? argv[1] : "solar42n-m4-fullpath.wav";
    const double seconds = !bench && argc > 2 ? std::atof(argv[2]) : 20.0;

    s42::Rack rack;
    rack.prepare(kSr, 512);

    // M6 audition scene — the drone bed from M3/M4 with the touch keyboard
    // as the melodic layer, all through the hardware normals:
    //   DRONE 1  beating 4-gen cluster (M1's voice), panned toward filter L;
    //   DRONE 4  a slow second cluster on the right filter, LFO A on its
    //            sensor LED via the CV jack (gens 1-2 MOD down);
    //   DRONE 3  Papa Srapa: FM+AM siren-bird chirps, low in the mix;
    //   KEYBOARD arp, HOLD-latched minor triad (plates 1/4/8), variation x1
    //            an octave up, portamento glide, clocked by the PATCHED
    //            PULSER (SeqClockOut -> kb CLOCK in — the M6 verify patch);
    //            V/OCT + GATE L reach both VCOs/envelopes via the normals;
    //   VCO A    saw, articulated by Envelope A from the keyboard gate;
    //   VCO B    default-normalled to VCO A's osc (CV AMT up = 2-op FM),
    //            slower Envelope B blooms under each arp note;
    //   Filters  L dark/resonant, R brighter + LFO B wobble on CV L (MOD L),
    //            DIST/GAIN warming everything, CATHEDRAL shimmer behind.
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
    c.envB.attackSec = 0.6f; // blooms under each arp note (gate via normal)
    c.envB.decaySec = 1.0f;
    c.envB.sustain = 0.5f;
    c.envB.releaseSec = 1.2f;

    // Touch keyboard (M6): HOLD-latched arp on a minor triad, one variation
    // pass an octave up, gliding, clocked from the pulser patch below.
    c.kb.side[0].mode = s42::KbMode::Arp;
    c.kb.side[0].arp.hold = true;
    c.kb.side[0].arp.clockRatio = s42::kKbClockUnity + 1; // x2 the pulser
    c.kb.side[0].arp.variation = 1;
    c.kb.side[0].arp.intervalSemis = 12;
    c.kb.portamento = 70;
    c.kb.vibSpeed = 60;
    c.kb.vibDepth = 18;
    c.kb.vibDelay = 50;
    c.kbTouch.plate[0] = c.kbTouch.plate[3] = c.kbTouch.plate[7] = 0.6f;

    c.seq.pulserHz = 2.2f; // the arp's clock source (patched into kb CLOCK)
    c.seq.stages = 5;

    c.lfoA.rateHz = 0.15f; c.lfoA.wave = 1.0f;  // slow triangle -> sensor LED
    c.lfoB.rateHz = 0.28f; c.lfoB.wave = 0.9f;  // filter wobble

    auto& m = c.mixer;
    for (float& v : m.vol) v = 0.0f;
    m.vol[s42::MixerModule::ChD1] = 0.55f;   m.pan[s42::MixerModule::ChD1] = 0.25f;
    m.vol[s42::MixerModule::ChD4] = 0.5f;    m.pan[s42::MixerModule::ChD4] = 0.75f;
    m.vol[s42::MixerModule::ChD3] = 0.22f;   m.pan[s42::MixerModule::ChD3] = 0.65f;
    m.vol[s42::MixerModule::ChVcoA] = 0.55f; m.pan[s42::MixerModule::ChVcoA] = 0.45f;
    m.vol[s42::MixerModule::ChVcoB] = 0.34f; m.pan[s42::MixerModule::ChVcoB] = 0.6f;

    c.filter.freqHzL = 950.0f;  c.filter.resL = 0.55f;
    c.filter.freqHzR = 1600.0f; c.filter.resR = 0.45f;
    c.filter.modL = 0.25f;      // LFO B wobble (patched below)
    c.filter.dist = 0.35f;
    c.filter.gain = 0.45f;

    // M4: dual FV-1 — CATHEDRAL shimmer on both channels, X = oct-up bloom,
    // Z = long decay, blended behind the analog path.
    c.fx.cartridge = 0;
    c.fx.progL = c.fx.progR = 0;
    c.fx.loadInserted(); // chips hold what the panel shows (M7 slot semantics)
    c.fx.x = 0.6f;
    c.fx.y = 0.15f;
    c.fx.z = 0.7f;
    c.fx.blend = 0.45f;

    c.roomLight = 0.25f;
    c.masterVol = 0.6f;
    rack.setControls(c);

    // Patch cables: pulser -> keyboard CLOCK (arp sync — the keyboard's
    // V/OCT and GATE L reach the VCOs/envelopes through the stock normals);
    // LFO A -> DRONE 4's sensor LED; LFO B -> filter CV L (CV R follows the
    // 42N normal).
    rack.requestPatch(s42::Inlet::KbClockIn, s42::Outlet::SeqClockOut);
    rack.requestPatch(s42::Inlet::D4CvIn, s42::Outlet::LfoAOut);
    rack.requestPatch(s42::Inlet::FiltCvLIn, s42::Outlet::LfoBOut);

    if (bench)
    {
        // The plan's reference config: 48 k / 128-sample blocks, fresh
        // controls every block — the exact per-block work processBlock does.
        constexpr int kBlock = 128;
        constexpr double kBenchSec = 10.0;
        const auto frames = (size_t) (kBenchSec * kSr);
        std::vector<float> l(frames + kBlock), r(frames + kBlock);

        for (size_t i = 0; i < (size_t) kSr; i += kBlock) // 1 s warm-up
        {
            rack.setControls(c);
            rack.process(l.data(), r.data(), nullptr, nullptr, kBlock);
        }
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < frames; i += kBlock)
        {
            rack.setControls(c);
            rack.process(l.data() + i, r.data() + i, nullptr, nullptr, kBlock);
        }
        const auto elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double pct = 100.0 * elapsed / kBenchSec;
        std::printf("solar42n_render --bench: %.0f s of audio in %.3f s"
                    " = %.2f%% of one core (48 kHz / %d-sample blocks)\n",
                    kBenchSec, elapsed, pct, kBlock);
        if (budgetPct > 0.0 && pct > budgetPct)
        {
            std::fprintf(stderr, "FAIL: %.2f%% exceeds the %.1f%% CPU budget\n",
                         pct, budgetPct);
            return 1;
        }
        return 0;
    }

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
    std::printf("solar42n_render: wrote %s (%.1f s, M6 full path: drones + srapa + "
                "keyboard arp (pulser-clocked, normalled into the VCOs) -> "
                "Polivoks + dist -> FV-1 shimmer)\n",
                out.c_str(), seconds);
    return 0;
}
