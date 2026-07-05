#include "engine/keyboard/TouchKeyboard.h"

#include "dsp/TuningConstants.h"
#include "engine/keyboard/Quantiser.h"

#include <cmath>

namespace s42 {

void TouchKeyboard::prepare(double sampleRate) noexcept
{
    sr_ = sampleRate;
    clock_.prepare(sampleRate);
    sides_[0] = Side {};
    sides_[1] = Side {};
    shaper_ = PressureShaper {};
    prevTouch_ = KbTouch {};
    prevButton_[0] = prevButton_[1] = false;
    lastResetHigh_ = false;
    vibPhase_ = 0.0f;
    octave_ = 0;
    offsetOn_[0] = offsetOn_[1] = false;
}

void TouchKeyboard::setConfig(const KbConfig& c) noexcept
{
    config_ = c;
    if (c.behaviour != lastBehaviour_)
    {
        // Re-splitting the plates invalidates held/latched state — release
        // everything rather than leave a stuck note on the wrong side.
        lastBehaviour_ = c.behaviour;
        for (Side& sd : sides_)
        {
            sd.held = sd.latched = 0;
            sd.orderLen = 0;
            sd.activePlate = -1;
            sd.gate = false;
            sd.pulse = 0.0f;
        }
        prevTouch_ = KbTouch {};
    }
}

// Quantise + transpose one side's raw pitch into the 0-8 V DAC range.
float TouchKeyboard::sidePitch(int side, float rawVolts) const noexcept
{
    float v = kbQuantise(rawVolts, config_.scaleMask, config_.rootNote);
    const bool quantised = config_.scaleMask != 0;
    if (config_.behaviour == KbBehaviour::Single)
    {
        if (quantised)
            v += (float) octave_; // buttons walk the active octave range
        else
        {
            if (offsetOn_[0]) v += config_.offsetVolts[0]; // offsets are signed
            if (offsetOn_[1]) v += config_.offsetVolts[1]; // (factory: -1 V / +1 V)
        }
    }
    else if (offsetOn_[side])
    {
        v += config_.offsetVolts[side];
    }
    return v < 0.0f ? 0.0f : (v > 8.0f ? 8.0f : v);
}

void TouchKeyboard::setNote(Side& sd, float rawVolts, bool allowGlide) noexcept
{
    sd.raw = rawVolts;
    if (!allowGlide || config_.portamento <= 0)
        sd.glide = sd.target; // target itself is refreshed every block
}

void TouchKeyboard::updateTouch(const KbTouch& touch) noexcept
{
    const int sides = numSides();
    const int count = plateCount();

    bool anyNow = false;
    for (int p = 0; p < 12; ++p)
        anyNow = anyNow || touch.plate[p] > kTouchThreshold;
    touchRose_ = anyNow && !anyTouch_;
    anyTouch_ = anyNow;

    for (int s = 0; s < sides; ++s)
    {
        Side& sd = sides_[s];
        const KbSideConfig& sc = sideCfg(s);

        uint16_t held = 0;
        float pressure = 0.0f;
        for (int local = 0; local < count; ++local)
        {
            const float p = touch.plate[globalPlate(s, local)];
            if (p > kTouchThreshold)
                held |= (uint16_t) (1u << local);
            pressure = p > pressure ? p : pressure;
        }
        sd.pressure = pressure;

        const uint16_t pressed = held & ~sd.held;
        const uint16_t released = sd.held & ~held;

        // Press-order bookkeeping (last-note priority).
        for (int local = 0; local < count; ++local)
        {
            if ((pressed >> local & 1) != 0 && sd.orderLen < 12)
                sd.order[sd.orderLen++] = local;
            if ((released >> local & 1) != 0)
            {
                int w = 0;
                for (int r = 0; r < sd.orderLen; ++r)
                    if (sd.order[r] != local)
                        sd.order[w++] = sd.order[r];
                sd.orderLen = w;
            }
        }

        // Arp hold latch (manual p16): while plates are physically held new
        // presses join the chord; a press after a full release replaces it.
        const bool arpHold = sc.mode == KbMode::Arp && sc.arp.hold;
        const uint16_t chordWas = arpHold ? sd.latched : sd.held;
        if (arpHold)
        {
            if (pressed != 0)
                sd.latched = sd.held == 0 ? pressed : (uint16_t) (sd.latched | pressed);
        }
        else
        {
            sd.latched = held;
        }

        // Track the newest plate: keyboard-mode note + sequencer transpose.
        if (pressed != 0 && sd.orderLen > 0)
            sd.plateV = plateRaw(touch, globalPlate(s, sd.order[sd.orderLen - 1]));

        sd.held = held;

        if (sc.mode == KbMode::Keyboard)
        {
            const int top = sd.orderLen > 0 ? sd.order[sd.orderLen - 1] : -1;
            if (top >= 0 && top != sd.activePlate)
            {
                // Legato: glide only when 2+ plates are touched together.
                const bool glide = !config_.legato || sd.orderLen >= 2;
                const float raw = plateRaw(touch, globalPlate(s, top));
                sd.target = sidePitch(s, raw);
                setNote(sd, raw, glide);
            }
            sd.activePlate = top;
            sd.gate = sd.orderLen > 0;
        }
        else
        {
            sd.activePlate = -1;
            if (sc.mode == KbMode::Arp)
            {
                const uint16_t chord = arpHold ? sd.latched : sd.held;
                if (chordWas == 0 && chord != 0)
                    sd.arp.reset(); // fresh chord starts from the first step
            }
        }

        // Vibrato delay restarts on a fresh touch of this side.
        if ((pressed != 0 && (sd.held & ~pressed) == 0))
            sd.vibEnv = 0.0f;
    }

    // Transpose buttons (manual p14).
    for (int b = 0; b < 2; ++b)
    {
        if (touch.button[b] && !prevButton_[b])
        {
            if (config_.behaviour == KbBehaviour::Single && config_.scaleMask != 0)
            {
                octave_ += b == 0 ? -1 : 1;
                octave_ = octave_ < -3 ? -3 : (octave_ > 4 ? 4 : octave_);
            }
            else
            {
                offsetOn_[b] = !offsetOn_[b];
            }
        }
        prevButton_[b] = touch.button[b];
    }

    prevTouch_ = touch;
}

void TouchKeyboard::process(const KbTouch& touch, const float* clockJack, bool clockPatched,
                            const float* resetJack, float* voct, float* gateL, float* gateR,
                            float* press, int n) noexcept
{
    updateTouch(touch);

    // Derived control values, refreshed once per sub-block.
    const float dt = (float) (1.0 / sr_);
    {
        const float p01 = (float) config_.portamento / 255.0f;
        const float tau = p01 * p01 * tuning::kPortamentoMaxSec;
        glideCoeff_ = config_.portamento <= 0 ? 1.0f
                                              : 1.0f - std::exp(-3.0f * dt / (tau + 1.0e-6f));
        const float s01 = (float) config_.vibSpeed / 127.0f;
        const float hz = tuning::kVibratoMinHz
                         + s01 * s01 * (tuning::kVibratoMaxHz - tuning::kVibratoMinHz);
        vibInc_ = hz * dt;
        vibDepthV_ = (float) config_.vibDepth / 127.0f * tuning::kVibratoMaxSemis / 12.0f;
        vibDelaySec_ = (float) config_.vibDelay / 127.0f * tuning::kVibratoDelayMaxSec;
        const float r01 = (float) config_.rise / 255.0f;
        const float f01 = (float) config_.fall / 255.0f;
        riseSec_ = r01 * r01 * tuning::kPressureSlewMaxSec;
        fallSec_ = f01 * f01 * tuning::kPressureSlewMaxSec;
    }

    const int sides = numSides();

    // Refresh pitch targets: quantiser/offset/octave edits move a sounding
    // note immediately, glide keeps heading for the fresh target.
    for (int s = 0; s < sides; ++s)
        sides_[s].target = sidePitch(s, sides_[s].raw);

    const bool single = config_.behaviour == KbBehaviour::Single;

    for (int i = 0; i < n; ++i)
    {
        const bool baseTick = clock_.tick(clockJack[i], clockPatched, config_.bpm, config_.bpmEdit);
        const float basePeriod = clock_.periodSamples(config_.bpm);

        const bool resetHigh = resetJack[i] > 2.5f;
        const bool resetEdge = resetHigh && !lastResetHigh_;
        lastResetHigh_ = resetHigh;

        for (int s = 0; s < sides; ++s)
        {
            Side& sd = sides_[s];
            const KbSideConfig& sc = sideCfg(s);

            if (sc.mode == KbMode::Arp)
            {
                if (resetEdge)
                    sd.arp.reset(); // RESET jack: back to the first step
                const bool t = sd.scaler.tick(baseTick, basePeriod, sc.arp.clockRatio);
                if (t)
                {
                    const uint16_t chord = sc.arp.hold ? sd.latched : sd.held;
                    if (sd.arp.tick(chord, sc.arp))
                    {
                        const float raw = plateRaw(prevTouch_, globalPlate(s, sd.arp.curPlate))
                                          + (float) sd.arp.curTransposeSemis / 12.0f;
                        sd.target = sidePitch(s, raw);
                        setNote(sd, raw, true);
                        sd.pulse = tuning::kKbGateDuty * basePeriod
                                   / kKbClockRatios[sc.arp.clockRatio];
                    }
                }
                sd.pulse = sd.pulse > 0.0f ? sd.pulse - 1.0f : 0.0f;
                sd.gate = sd.pulse > 0.0f;
            }
            else if (sc.mode == KbMode::Seq)
            {
                const bool t = sd.scaler.tick(baseTick, basePeriod, sc.seq.clockRatio);
                if (t && (!sc.seq.keyRun || sd.held != 0)) // key-run: advance only while held
                {
                    if (sd.seq.tick(sc.seq))
                    {
                        const int st = sd.seq.curStep;
                        const bool g = sc.seq.gate[st];
                        if (!sc.seq.gatedCv || g)
                            sd.seqCv = sc.seq.value[st];
                        const float raw = sd.plateV + sd.seqCv; // transposed by the active plate
                        sd.target = sidePitch(s, raw);
                        setNote(sd, raw, true);
                        if (g)
                            sd.pulse = tuning::kKbGateDuty * basePeriod
                                       / kKbClockRatios[sc.seq.clockRatio];
                    }
                }
                sd.pulse = sd.pulse > 0.0f ? sd.pulse - 1.0f : 0.0f;
                sd.gate = sd.pulse > 0.0f;
            }
            // Keyboard mode: gate set at block rate in updateTouch().

            // Portamento (one-pole slew toward the target).
            sd.glide += (sd.target - sd.glide) * glideCoeff_;

            // Vibrato depth ramps in over the delay time from note start.
            if (sd.gate && vibDelaySec_ > 1.0e-4f)
                sd.vibEnv += dt / vibDelaySec_;
            else if (sd.gate)
                sd.vibEnv = 1.0f;
            sd.vibEnv = sd.vibEnv > 1.0f ? 1.0f : sd.vibEnv;
        }

        vibPhase_ += vibInc_;
        if (vibPhase_ >= 1.0f)
            vibPhase_ -= 1.0f;
        const float lfo = std::sin(vibPhase_ * 6.28318530718f);

        auto pitchOut = [&](int s) noexcept
        {
            const Side& sd = sides_[s];
            float depth = vibDepthV_ * sd.vibEnv;
            if (config_.vibPressure)
                depth *= sd.pressure;
            const float v = sd.glide + lfo * depth;
            return v < 0.0f ? 0.0f : (v > 8.0f ? 8.0f : v);
        };

        voct[i] = pitchOut(0);
        gateL[i] = sides_[0].gate ? kGateVolts : 0.0f;
        if (single)
        {
            gateR[i] = gateL[i];
            press[i] = shaper_.process(sides_[0].pressure, anyTouch_, touchRose_ && i == 0,
                                       config_.pressureMode, riseSec_, fallSec_, dt);
        }
        else
        {
            gateR[i] = sides_[1].gate ? kGateVolts : 0.0f;
            press[i] = pitchOut(1); // twin/split: PRESSURE is the right side's V/oct
        }
    }
}

uint16_t TouchKeyboard::heldMask() const noexcept
{
    if (config_.behaviour == KbBehaviour::Single)
        return (uint16_t) (sides_[0].held | sides_[0].latched);
    return (uint16_t) ((sides_[0].held | sides_[0].latched)
                       | ((sides_[1].held | sides_[1].latched) << 6));
}

int TouchKeyboard::stepOf(int side) const noexcept
{
    const KbSideConfig& sc = sideCfg(side);
    if (sc.mode == KbMode::Seq)
        return sides_[side & 1].seq.curStep;
    if (sc.mode == KbMode::Arp)
        return sides_[side & 1].arp.pos;
    return -1;
}

} // namespace s42
