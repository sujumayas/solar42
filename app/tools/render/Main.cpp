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
    const std::string out = argc > 1 ? argv[1] : "solar42n-m1-drone1.wav";
    const double seconds = argc > 2 ? std::atof(argv[2]) : 12.0;

    s42::Rack rack;
    rack.prepare(kSr, 512);

    // M1 audition patch: DRONE 1, gens 1-3 as a beating low cluster with the
    // 4th an octave up, HOLD latched; slow swell. Last third: VOLT pushed into
    // the cross-FM dirty zone.
    s42::Rack::Controls c;
    c.drone1.tune[0] = 0.55f; // gen1 in its C0-G2 band
    c.drone1.tune[1] = 0.32f; // gen2 near gen1 -> slow beating
    c.drone1.tune[2] = 0.22f; // gen3 a loose fifth up
    c.drone1.tune[3] = 0.35f; // gen4 octave region
    c.drone1.mute[4] = true;  // top gen off
    c.drone1.volt = 0.0f;
    c.drone1.attackSec = 2.5f;
    c.drone1.releaseSec = 6.0f;
    c.drone1.hold = true;
    c.filterFreqHz = 1200.0f;
    c.filterRes = 0.45f;
    c.masterVol = 0.6f;
    rack.setControls(c);

    const auto total = (size_t) (seconds * kSr);
    std::vector<float> l(total), r(total);

    const size_t dirtyAt = total * 2 / 3;
    for (size_t i = 0; i < total; i += 512)
    {
        if (i >= dirtyAt && c.drone1.volt < 0.9f)
        {
            // creep VOLT into the dirty zone over the last third
            c.drone1.volt = 0.55f
                + 0.35f * (float) (i - dirtyAt) / (float) (total - dirtyAt);
            rack.setControls(c);
        }
        const auto n = std::min<size_t>(512, total - i);
        rack.process(l.data() + i, r.data() + i, nullptr, nullptr, (int) n);
    }

    writeWav16(out, l, r);
    std::printf("solar42n_render: wrote %s (%.1f s, DRONE 1 cluster -> VOLT dirty zone)\n",
                out.c_str(), seconds);
    return 0;
}
