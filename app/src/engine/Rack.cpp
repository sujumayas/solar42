#include "engine/Rack.h"

#include "dsp/TuningConstants.h"
#include "dsp/Volts.h"
#include "dsp/Waveshaper.h"

#include <algorithm>
#include <cmath>

namespace s42 {

namespace
{
// Per-voice jack lookup, index-aligned with Rack's voice arrays
// (classic = DRONE 1, 2, 4, 5; srapa = DRONE 3, 6).
constexpr Inlet kDroneGateIn[] = { Inlet::D1GateIn, Inlet::D2GateIn, Inlet::D4GateIn, Inlet::D5GateIn };
constexpr Inlet kDroneCvIn[] = { Inlet::D1CvIn, Inlet::D2CvIn, Inlet::D4CvIn, Inlet::D5CvIn };
constexpr Outlet kDroneEnvOut[] = { Outlet::D1EnvOut, Outlet::D2EnvOut, Outlet::D4EnvOut, Outlet::D5EnvOut };
constexpr int kDroneNumber[] = { 1, 2, 4, 5 };

constexpr Inlet kSrapaGateIn[] = { Inlet::D3GateIn, Inlet::D6GateIn };
constexpr Inlet kSrapaShIn[] = { Inlet::D3ShIn, Inlet::D6ShIn };
constexpr Inlet kSrapaShClockIn[] = { Inlet::D3ShClockIn, Inlet::D6ShClockIn };
constexpr Outlet kSrapaCvOut[] = { Outlet::D3CvOut, Outlet::D6CvOut };
constexpr Outlet kSrapaEnvOut[] = { Outlet::D3EnvOut, Outlet::D6EnvOut };
constexpr Outlet kSrapaShOut[] = { Outlet::D3ShOut, Outlet::D6ShOut };
constexpr int kSrapaNumber[] = { 3, 6 };
} // namespace

void Rack::prepare(double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_ = sampleRate;

    keyboard_.prepare(sampleRate);
    lfoA_.prepare(sampleRate);
    lfoB_.prepare(sampleRate);
    joy_.prepare(sampleRate);
    seq_.prepare(sampleRate);
    preamp_.prepare(sampleRate);

    for (int v = 0; v < kClassicVoices; ++v)
    {
        drones_[v].prepare(sampleRate, tolerances_, kDroneNumber[v]);
        sensors_[v].prepare(sampleRate);
    }
    for (int s = 0; s < kSrapaVoices; ++s)
        srapas_[s].prepare(sampleRate, tolerances_, kSrapaNumber[s]);

    vcoA_.prepare(sampleRate, tolerances_, "vcoA");
    vcoB_.prepare(sampleRate, tolerances_, "vcoB");
    envA_.prepare(sampleRate);
    envB_.prepare(sampleRate);

    mixer_.prepare(sampleRate);
    filterL_.setSampleRate(sampleRate);
    filterR_.setSampleRate(sampleRate);
    filterL_.seedNoise(tolerances_.serial() ^ 0x1EFFull);
    filterR_.seedNoise(tolerances_.serial() ^ 0x21617ull);
    effector_.prepare(sampleRate, tolerances_);
    masterSm_.prepare(sampleRate, 0.01f, controls_.masterVol);

    bus_.clearAll();
    setControls(controls_);
}

void Rack::setControls(const Controls& c) noexcept
{
    controls_ = c;

    keyboard_.setConfig(c.kb);
    lfoA_.setParams(c.lfoA);
    lfoB_.setParams(c.lfoB);
    joy_.setParams(c.joy);
    seq_.setParams(c.seq);
    preamp_.setParams(c.preamp);

    for (int v = 0; v < kClassicVoices; ++v)
    {
        drones_[v].setParams(c.drone[v]);
        sensors_[v].setRoom(c.roomLight, c.mainsFlicker);
    }
    for (int s = 0; s < kSrapaVoices; ++s)
        srapas_[s].setParams(c.srapa[s]);

    vcoA_.setParams(c.vcoA);
    vcoB_.setParams(c.vcoB);
    envA_.setParams(c.envA);
    envB_.setParams(c.envB);
    mixer_.setParams(c.mixer);

    // "Live stereo": both filter channels pull fixed component-tolerance
    // offsets — cutoff and resonance (so the self-osc onset differs L/R too).
    filterFreqL_ = c.filter.freqHzL * tolerances_.spread(Tolerances::id("filtL.freq"), tuning::kFilterLrSkew);
    filterFreqR_ = c.filter.freqHzR * tolerances_.spread(Tolerances::id("filtR.freq"), tuning::kFilterLrSkew);
    filterL_.setRes(c.filter.resL * tolerances_.spread(Tolerances::id("filtL.res"), tuning::kFilterLrSkew));
    filterR_.setRes(c.filter.resR * tolerances_.spread(Tolerances::id("filtR.res"), tuning::kFilterLrSkew));
    filterL_.setMode(c.filter.bpL);
    filterR_.setMode(c.filter.bpR);
    effector_.setParams(c.fx);

    masterSm_.setTarget(c.masterVol);
}

void Rack::drainCommands() noexcept
{
    PatchCmd cmd;
    while (cmdQueue_.pop(cmd))
    {
        if (cmd.outlet == VoltBus::kNoSource)
            bus_.unpatch((Inlet) cmd.inlet);
        else
            bus_.patch((Inlet) cmd.inlet, (Outlet) cmd.outlet);
    }
}

void Rack::process(float* outL, float* outR, const float* extInL, const float* extInR,
                   int numSamples) noexcept
{
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min(kSubBlock, numSamples - done);
        processSubBlock(outL + done, outR + done,
                        extInL != nullptr ? extInL + done : nullptr,
                        extInR != nullptr ? extInR + done : nullptr, n);
        done += n;
    }
}

void Rack::processSubBlock(float* outL, float* outR, const float* extInL,
                           const float* extInR, int n) noexcept
{
    drainCommands();

    // Fixed hardware-mirroring order (backward cables read the previous
    // sub-block — that's the feedback semantics, see VoltBus). The keyboard
    // runs first: its V/oct + gates feed the VCO/envelope normals.
    keyboard_.process(controls_.kbTouch,
                      bus_.in(Inlet::KbClockIn), bus_.isPatched(Inlet::KbClockIn),
                      bus_.in(Inlet::KbResetIn),
                      bus_.out(Outlet::KbVoctOut), bus_.out(Outlet::KbGateLOut),
                      bus_.out(Outlet::KbGateROut), bus_.out(Outlet::KbPressOut), n);
    lfoA_.process(bus_, Outlet::LfoAOut, n);
    lfoB_.process(bus_, Outlet::LfoBOut, n);
    joy_.process(bus_, n);
    seq_.process(bus_, n);
    preamp_.process(bus_, extInL, n);

    // EXT AUDIO mixer channel: the host input by default (HostInput normal),
    // or whatever cable lands in the jack.
    float extV[kSubBlock];
    if (bus_.isPatched(Inlet::ExtAudioIn))
    {
        const float* p = bus_.in(Inlet::ExtAudioIn);
        for (int i = 0; i < n; ++i)
            extV[i] = p[i];
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            float v = extInL != nullptr ? extInL[i] : 0.0f;
            if (extInR != nullptr)
                v = 0.5f * (v + extInR[i]);
            extV[i] = v * tuning::kExtAudioVolts;
        }
    }

    // Classic drones (DRONE 1, 2, 4, 5). The CV jack drives the voice's
    // photo-sensor LED; the LDR (plus room light) is the pitch-mod bus for
    // whichever generators have MOD down.
    float droneOut[kClassicVoices][kSubBlock];
    for (int v = 0; v < kClassicVoices; ++v)
    {
        const float* gateJack = bus_.in(kDroneGateIn[v]);
        const float* cvJack = bus_.in(kDroneCvIn[v]);
        float* envOut = bus_.out(kDroneEnvOut[v]);

        for (int i = 0; i < n; ++i)
        {
            const float mod = sensors_[v].process(railLimit(cvJack[i]));
            const bool gate = controls_.droneKey[v] || gateJack[i] > 2.5f;
            droneOut[v][i] = drones_[v].process(gate, mod);
            envOut[i] = drones_[v].envOutVolts();
        }
    }

    // Papa Srapa voices (DRONE 3, 6).
    float srapaOut[kSrapaVoices][kSubBlock];
    for (int s = 0; s < kSrapaVoices; ++s)
    {
        const float* gateJack = bus_.in(kSrapaGateIn[s]);
        const float* shIn = bus_.in(kSrapaShIn[s]);
        const float* shClock = bus_.in(kSrapaShClockIn[s]);
        const bool shPatched = bus_.isPatched(kSrapaShIn[s]);
        float* cvOut = bus_.out(kSrapaCvOut[s]);
        float* envOut = bus_.out(kSrapaEnvOut[s]);
        float* shOut = bus_.out(kSrapaShOut[s]);

        for (int i = 0; i < n; ++i)
        {
            const bool gate = controls_.srapaKey[s] || gateJack[i] > 2.5f;
            srapaOut[s][i] = srapas_[s].process(gate, shPatched,
                                                railLimit(shIn[i]), shClock[i]);
            cvOut[i] = srapas_[s].cvOutVolts();
            envOut[i] = srapas_[s].envOutVolts();
            shOut[i] = srapas_[s].shOutVolts();
        }
    }

    // VCO A first — its raw output is the (hidden) outlet normalled into
    // VCO B's CV jack, so B reads current data in the same sub-block.
    float* aOut = bus_.out(Outlet::VcoAOscInt);
    {
        const float* voct = bus_.in(Inlet::VcoAVoctIn);
        const float* cv = bus_.in(Inlet::VcoACvIn);
        const float* pwm = bus_.in(Inlet::VcoAPwmIn);
        const float* sync = bus_.in(Inlet::VcoASyncIn);
        for (int i = 0; i < n; ++i)
            aOut[i] = vcoA_.process(voct[i], cv[i], pwm[i], sync[i]);
    }
    float* bOut = bus_.out(Outlet::VcoBOscOut);
    {
        const float* voct = bus_.in(Inlet::VcoBVoctIn);
        const float* cv = bus_.in(Inlet::VcoBCvIn);
        const float* pwm = bus_.in(Inlet::VcoBPwmIn);
        for (int i = 0; i < n; ++i)
            bOut[i] = vcoB_.process(voct[i], cv[i], pwm[i], 0.0f); // no sync jack on B
    }

    // DRY OUT jacks (spec p4: max 1 V from the +-5 V oscillators).
    {
        float* aDry = bus_.out(Outlet::VcoADryOut);
        float* bDry = bus_.out(Outlet::VcoBDryOut);
        for (int i = 0; i < n; ++i)
        {
            aDry[i] = aOut[i] * 0.2f;
            bDry[i] = bOut[i] * 0.2f;
        }
    }

    // Envelope A/B: the VCAs that carry the VCOs into the mixer.
    float vcoAMix[kSubBlock], vcoBMix[kSubBlock];
    {
        const float* gateA = bus_.in(Inlet::EnvAGateIn);
        const float* vcaCvA = bus_.in(Inlet::EnvAVcaCvIn);
        const float* gateB = bus_.in(Inlet::EnvBGateIn);
        const float* vcaCvB = bus_.in(Inlet::EnvBVcaCvIn);
        float* envAOut = bus_.out(Outlet::EnvAOut);
        float* envBOut = bus_.out(Outlet::EnvBOut);

        for (int i = 0; i < n; ++i)
        {
            vcoAMix[i] = aOut[i] * envA_.process(gateA[i] > 2.5f, railLimit(vcaCvA[i]));
            envAOut[i] = envA_.envOutVolts();
            vcoBMix[i] = bOut[i] * envB_.process(gateB[i] > 2.5f, railLimit(vcaCvB[i]));
            envBOut[i] = envB_.envOutVolts();
        }
    }

    // Mixer (pan = filter routing) -> dual Polivoks -> DIST/GAIN -> dual
    // FV-1 effector -> master.
    const float* cvL = bus_.in(Inlet::FiltCvLIn);
    const float* cvR = bus_.in(Inlet::FiltCvRIn);
    const float* fxX = bus_.in(Inlet::FxCvXIn);
    const float* fxY = bus_.in(Inlet::FxCvYIn);
    const float* fxZ = bus_.in(Inlet::FxCvZIn);
    const FilterParams& f = controls_.filter;
    float peakL = 0.0f, peakR = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float ch[MixerModule::kChannels] = {
            droneOut[0][i], droneOut[1][i], srapaOut[0][i], extV[i],
            vcoAMix[i], vcoBMix[i], preamp_.preampBus[i],
            droneOut[2][i], droneOut[3][i], srapaOut[1][i]
        };
        float busL, busR;
        mixer_.process(ch, busL, busR);
        busL *= tuning::kMixerPostGain;
        busR *= tuning::kMixerPostGain;

        // MOD L/R scale the CV jacks; LINK slaves channel R to CV L's product.
        const float cl = railLimit(cvL[i]) * f.modL;
        const float cr = f.link ? cl : railLimit(cvR[i]) * f.modR;
        const float fcL = filterFreqL_ * std::exp2(cl * tuning::kFilterModOctPerVolt);
        const float fcR = filterFreqR_ * std::exp2(cr * tuning::kFilterModOctPerVolt);

        float l = filterL_.process(railClamp(busL), fcL);
        float r = filterR_.process(railClamp(busR), fcR);
        l = distGain(l, f.gain, f.dist);
        r = distGain(r, f.gain, f.dist);

        effector_.process(l, r, railLimit(fxX[i]), railLimit(fxY[i]), railLimit(fxZ[i]));

        const float master = masterSm_.next();
        outL[i] = railClamp(l) * kVoltsToFs * master;
        outR[i] = railClamp(r) * kVoltsToFs * master;
        peakL = std::max(peakL, std::abs(outL[i]));
        peakR = std::max(peakR, std::abs(outR[i]));
    }

    publishTelemetry(peakL, peakR, n);
}

void Rack::publishTelemetry(float peakL, float peakR, int n) noexcept
{
    TelemetryData t;
    for (int v = 0; v < kClassicVoices; ++v)
    {
        const float env = drones_[v].envOutVolts() * 0.1f;
        for (int g = 0; g < ClassicDroneVoice::kNumGens; ++g)
            t.droneGen[v][g] = drones_[v].genLevel(g) * env;
        t.droneEnv[v] = env;
        t.sensor[v] = sensors_[v].brightness();
    }
    for (int s = 0; s < kSrapaVoices; ++s)
    {
        t.srapaCv[s] = srapas_[s].cvOutVolts() * (1.0f / 12.0f);
        t.srapaEnv[s] = srapas_[s].envOutVolts() * 0.1f;
    }
    t.envA = envA_.envOutVolts() * (1.0f / 8.0f);
    t.envB = envB_.envOutVolts() * (1.0f / 8.0f);
    t.lfoA = bus_.outRead(Outlet::LfoAOut)[n - 1] * 0.1f;
    t.lfoB = bus_.outRead(Outlet::LfoBOut)[n - 1] * 0.1f;
    t.seqStep = seq_.currentStep();
    t.seqGate = bus_.outRead(Outlet::SeqGateOut)[n - 1] > 1.0f ? 1.0f : 0.0f;
    float preampPeak = 0.0f;
    for (int i = 0; i < n; ++i)
        preampPeak = std::max(preampPeak, std::abs(preamp_.preampBus[i]));
    t.preampClip = preampPeak > 4.9f ? 1.0f : 0.0f;
    t.follower = bus_.outRead(Outlet::EnvFEnvOut)[n - 1] * 0.1f;
    t.followerGate = bus_.outRead(Outlet::EnvFGateOut)[n - 1] > 1.0f ? 1.0f : 0.0f;
    t.kbGateL = bus_.outRead(Outlet::KbGateLOut)[n - 1] > 1.0f ? 1.0f : 0.0f;
    t.kbGateR = bus_.outRead(Outlet::KbGateROut)[n - 1] > 1.0f ? 1.0f : 0.0f;
    t.kbVoct = bus_.outRead(Outlet::KbVoctOut)[n - 1];
    t.kbPress = bus_.outRead(Outlet::KbPressOut)[n - 1];
    t.kbHeld = keyboard_.heldMask();
    t.kbStep[0] = keyboard_.stepOf(0);
    t.kbStep[1] = keyboard_.stepOf(1);
    t.kbExtClock = keyboard_.usingExternalClock() ? 1.0f : 0.0f;
    t.kbOctave = keyboard_.octaveShift();
    t.kbOffset[0] = keyboard_.offsetEnabled(0) ? 1.0f : 0.0f;
    t.kbOffset[1] = keyboard_.offsetEnabled(1) ? 1.0f : 0.0f;
    t.outL = peakL;
    t.outR = peakR;
    telemetry_.publish(t);
}

} // namespace s42
