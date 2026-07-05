// solar42n_statecheck — the M7 verify harness, run against the REAL
// processor (not just the engine):
//
//   1. "DAW kill/relaunch": randomize every parameter, break normals with
//      cables, reconfigure the keyboard, capture a mid-swap cartridge slot,
//      save -> fresh instance -> restore -> the two must match numerically
//      AND render sample-identical audio.
//   2. Every factory preset loads through the click-free path and renders
//      finite, audible sound (Init is allowed to be silent).
//   3. Click-free switch: loading a preset mid-render must fade (level dip)
//      instead of stepping (no hard discontinuity).
//
// Also doubles as the preset audition renderer:
//   solar42n_statecheck --render <factory-index> <out.wav> <seconds>
#include "plugin/PluginProcessor.h"
#include "state/PresetManager.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;

#define CHECK(cond, msg)                                                        \
    do                                                                          \
    {                                                                           \
        if (cond)                                                               \
            std::printf("  ok    %s\n", msg);                                   \
        else                                                                    \
        {                                                                       \
            std::printf("  FAIL  %s\n", msg);                                   \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

constexpr double kSr = 48000.0;

void pump(int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);
}

// Settle the smoother-priming path so two instances render identically:
// prepare -> one block (setControls latches the real params into the rack)
// -> prepare again (everything re-primes FROM those params).
void stabilize(Solar42NProcessor& p, int blockSize)
{
    juce::AudioBuffer<float> buf(2, blockSize);
    juce::MidiBuffer midi;
    p.prepareToPlay(kSr, blockSize);
    buf.clear();
    p.processBlock(buf, midi);
    p.prepareToPlay(kSr, blockSize);
}

std::vector<float> render(Solar42NProcessor& p, int blockSize, int numBlocks,
                          bool pumpBetweenBlocks = false)
{
    juce::AudioBuffer<float> buf(2, blockSize);
    juce::MidiBuffer midi;
    std::vector<float> out;
    out.reserve((size_t) (blockSize * numBlocks * 2));
    for (int b = 0; b < numBlocks; ++b)
    {
        if (pumpBetweenBlocks)
            pump(3);
        buf.clear();
        p.processBlock(buf, midi);
        for (int i = 0; i < blockSize; ++i)
        {
            out.push_back(buf.getSample(0, i));
            out.push_back(buf.getSample(1, i));
        }
    }
    return out;
}

double rmsOf(const std::vector<float>& x, size_t from, size_t to)
{
    double acc = 0.0;
    for (size_t i = from; i < to && i < x.size(); ++i)
        acc += (double) x[i] * x[i];
    return std::sqrt(acc / (double) juce::jmax((size_t) 1, to - from));
}

bool allFinite(const std::vector<float>& x)
{
    for (float v : x)
        if (!std::isfinite(v))
            return false;
    return true;
}

void writeWav16(const juce::String& path, const std::vector<float>& interleaved)
{
    const uint32_t frames = (uint32_t) (interleaved.size() / 2);
    const uint16_t channels = 2, bits = 16;
    const uint32_t sampleRate = (uint32_t) kSr;
    const uint32_t dataBytes = frames * channels * bits / 8;

    FILE* f = std::fopen(path.toRawUTF8(), "wb");
    if (f == nullptr)
    {
        std::printf("cannot open %s\n", path.toRawUTF8());
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
    u32(sampleRate * channels * bits / 8);
    u16((uint16_t) (channels * bits / 8));
    u16(bits);
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    for (float v : interleaved)
    {
        const auto s = (int16_t) (juce::jlimit(-1.0f, 1.0f, v) * 32767.0f);
        std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
}

// ---------------------------------------------------------------- test 1
void testRoundTrip()
{
    std::printf("== round trip: randomized rig -> save -> fresh instance -> restore ==\n");

    Solar42NProcessor a;
    juce::Random rng(0x50DA42);
    for (auto* p : a.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
        {
            // Snap through the parameter's own range (bools/choices hold the
            // raw normalized float in memory but persist the snapped value).
            const float snapped =
                rp->convertTo0to1(rp->convertFrom0to1(rng.nextFloat()));
            rp->setValueNotifyingHost(snapped);
        }

    // Keep the rig audible through the full chain regardless of the dice.
    auto force = [&a](const char* id, float norm)
    {
        if (auto* rp = a.apvts().getParameter(id))
            rp->setValueNotifyingHost(norm);
    };
    force("master.vol", 0.8f);
    force("mix.d1.vol", 0.8f);
    force("d1.hold", 1.0f);
    force("d1.gen1.mute", 0.0f);
    force("d1.gen2.mute", 0.0f);
    force("fx.blend", 0.5f);
    force("fx.cart", 1.0f / 3.0f); // TIME inserted (choice index 1 of 4)
    force("fx.progL", 1.0f);       // toggle at 3

    // Cables: break the keyboard/VCO normal, patch mod + clock paths.
    a.patchBay().connect(s42::Inlet::VcoAVoctIn, s42::Outlet::SeqCvOut);
    a.patchBay().connect(s42::Inlet::D1CvIn, s42::Outlet::LfoBOut);
    a.patchBay().connect(s42::Inlet::KbClockIn, s42::Outlet::SeqClockOut);
    a.patchBay().connect(s42::Inlet::FiltCvLIn, s42::Outlet::JoyXOut);

    // Keyboard config well off the defaults (incl. microtonal plate tuning).
    auto cfg = a.keyboardState().config();
    cfg.behaviour = s42::KbBehaviour::Split;
    cfg.side[0].mode = s42::KbMode::Arp;
    cfg.side[0].arp.hold = true;
    cfg.side[0].arp.variation = 2;
    cfg.scaleMask = 0;      // quantiser off = microtonal plates
    cfg.plateVolts[3] = 1.2345f;
    cfg.plateVolts[7] = 3.21f;
    cfg.portamento = 88;
    a.keyboardState().set(cfg);

    // Latch the toggles against TIME by actually processing, then swap the
    // inserted cartridge WITHOUT flipping — the saved state must capture the
    // mid-swap slot (chips on TIME/3 while OCHRE sits in the bay).
    stabilize(a, 512);
    render(a, 512, 8);
    force("fx.cart", 1.0f); // OCHRE inserted, no toggle flip
    render(a, 512, 8);

    juce::MemoryBlock blob;
    a.getStateInformation(blob);

    Solar42NProcessor b;
    b.setStateInformation(blob.getData(), (int) blob.getSize());

    // Numeric compare: every parameter, then the state children.
    {
        const auto& pa = a.getParameters();
        const auto& pb = b.getParameters();
        bool same = pa.size() == pb.size();
        for (int i = 0; same && i < pa.size(); ++i)
            if (std::abs(pa[i]->getValue() - pb[i]->getValue()) >= 1.0e-6f)
            {
                same = false;
                std::printf("  first mismatch: '%s'  a=%.8f  b=%.8f\n",
                            pa[i]->getName(64).toRawUTF8(),
                            pa[i]->getValue(), pb[i]->getValue());
            }
        CHECK(same, "all parameter values restored");

        auto childEq = [&](const char* name)
        {
            const auto ca = a.apvts().state.getChildWithName(name);
            const auto cb = b.apvts().state.getChildWithName(name);
            return ca.isValid() && cb.isValid() && ca.isEquivalentTo(cb);
        };
        CHECK(childEq("CABLES"), "CABLES child restored");
        CHECK(childEq("KEYBOARD"), "KEYBOARD child restored (incl. microtuning)");
        CHECK(b.currentFullState().getChildWithName("CARTRIDGES")
                  .isEquivalentTo(a.currentFullState().getChildWithName("CARTRIDGES")),
              "CARTRIDGES child restored (mid-swap slot survives)");
        CHECK(a.unitSerial() == b.unitSerial(), "unit serial restored");
    }

    // The actual "identical sound" gate — relaunch vs relaunch: free-running
    // phase history can't survive a kill (hardware can't do that either), so
    // the guarantee is that two fresh instances restored from the same blob
    // are sample-identical.
    Solar42NProcessor c;
    c.setStateInformation(blob.getData(), (int) blob.getSize());
    stabilize(b, 512);
    stabilize(c, 512);
    const auto rb = render(b, 512, 94); // ~1 s
    const auto rc = render(c, 512, 94);
    CHECK(allFinite(rb) && allFinite(rc), "renders finite");
    CHECK(rb == rc, "relaunched instances render sample-identical audio");
    CHECK(rmsOf(rb, rb.size() / 2, rb.size()) > 1.0e-5, "round-trip rig is audible");
}

// ---------------------------------------------------------------- test 2
void testFactoryPresets()
{
    std::printf("== factory presets load + render ==\n");
    Solar42NProcessor p;
    p.prepareToPlay(kSr, 512);
    auto& pm = p.presets();

    for (int i = 0; i < pm.numFactory(); ++i)
    {
        pm.loadFactory(i);
        pump(120); // the faded apply lands ~35 ms after the request
        stabilize(p, 512);
        const auto out = render(p, 512, 4 * 94); // ~4 s (slow drone attacks)
        const auto name = pm.factoryName(i);
        const double tail = rmsOf(out, out.size() * 3 / 4, out.size());
        std::printf("  preset %d '%s': tail RMS %.5f\n", i, name.toRawUTF8(), tail);
        CHECK(allFinite(out), ("preset renders finite: " + name).toRawUTF8());
        if (i > 0) // Init is legitimately silent until played
            CHECK(tail > 1.0e-4, ("preset is audible: " + name).toRawUTF8());
    }
}

// ---------------------------------------------------------------- test 3
void testClickFreeSwitch()
{
    std::printf("== click-free preset switch ==\n");
    Solar42NProcessor p;
    p.prepareToPlay(kSr, 256);
    auto& pm = p.presets();

    pm.loadFactory(1); // Cathedral Bloom
    pump(120);
    stabilize(p, 256);
    render(p, 256, 560); // ~3 s: let the drones swell

    // Render across the switch, pumping the message loop between blocks so
    // the fade -> apply -> fade-back sequence executes mid-capture.
    juce::AudioBuffer<float> buf(2, 256);
    juce::MidiBuffer midi;
    std::vector<float> l;
    for (int b = 0; b < 280; ++b) // ~1.5 s
    {
        if (b == 40)
            pm.loadFactory(3); // Srapa Aviary
        pump(3);
        buf.clear();
        p.processBlock(buf, midi);
        for (int i = 0; i < 256; ++i)
            l.push_back(buf.getSample(0, i));
    }

    CHECK(allFinite(l), "switch capture finite");

    const double preRms = rmsOf(l, 0, 256 * 30);
    CHECK(preRms > 1.0e-3, "audible before the switch");

    // The fade must dip the level to near-silence somewhere in the window...
    double minWin = 1.0e9;
    const size_t win = 240; // 5 ms
    for (size_t i = 256 * 40; i + win < l.size() && i < 256 * 120; i += win / 2)
        minWin = std::min(minWin, rmsOf(l, i, i + win));
    std::printf("  min 5 ms window RMS across the swap: %.6f\n", minWin);
    CHECK(minWin < 3.0e-3, "fade dips to near-silence (no hard state cut)");

    // ...and no sample step may exceed a hard-cut-sized jump.
    float maxStep = 0.0f;
    for (size_t i = 1; i < l.size(); ++i)
        maxStep = std::max(maxStep, std::abs(l[i] - l[i - 1]));
    std::printf("  max sample step across the swap: %.4f\n", maxStep);
    CHECK(maxStep < 0.3f, "no hard discontinuity across the swap");
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc >= 3 && juce::String(argv[1]) == "--render")
    {
        const int idx = std::atoi(argv[2]);
        const juce::String out = argc > 3 ? argv[3] : "preset.wav";
        const double seconds = argc > 4 ? std::atof(argv[4]) : 20.0;

        Solar42NProcessor p;
        p.prepareToPlay(kSr, 512);
        p.presets().loadFactory(idx);
        pump(120);
        stabilize(p, 512);
        const auto audio = render(p, 512, (int) (seconds * kSr / 512.0));
        writeWav16(out, audio);
        std::printf("statecheck: rendered factory preset %d '%s' -> %s (%.1f s)\n",
                    idx, p.presets().factoryName(idx).toRawUTF8(), out.toRawUTF8(),
                    seconds);
        return 0;
    }

    testRoundTrip();
    testFactoryPresets();
    testClickFreeSwitch();

    std::printf(failures == 0 ? "statecheck: ALL GREEN\n"
                              : "statecheck: %d FAILURE(S)\n",
                failures);
    return failures == 0 ? 0 : 1;
}
