#pragma once

#include "dsp/PolyphaseResampler.h"
#include "dsp/Smoother.h"
#include "dsp/Tolerances.h"
#include "dsp/TuningConstants.h"
#include "fv1/CartridgeLibrary.h"
#include "fv1/Fv1Vm.h"

#include <algorithm>

namespace s42 {

// DUAL EFFECTOR: two FV-1 chips (one per filter channel), each running one
// program of the inserted cartridge, selected by that channel's 1-2-3 toggle.
// X/Y/Z knobs + CV jacks are shared by both chips (POTn = knob + CV/20,
// ~20 ms RC smoothing, 9-bit converter quantization inside the VM); BLEND
// crossfades the analog dry path against the chip returns. Channel R's
// crystal is skewed +-0.05 % from the unit serial — two real chips never
// share a clock ("live stereo").
//
// Hardware slot semantics: flipping a channel's toggle loads that program
// from the *currently inserted* cartridge (RAM cleared, LFOs jammed);
// swapping the cartridge alone changes nothing until a toggle flips. The
// loaded pair per channel is therefore state of its own (M7): the caller
// tracks toggle flips and passes what each chip should currently HOLD via
// the loaded* fields — which may name a different cartridge than the
// inserted one, and persists/restores exactly.
class EffectorModule
{
public:
    struct Params
    {
        int cartridge = 0;         // inserted cartridge (library order)
        int progL = 0, progR = 0;  // 1-2-3 toggle positions
        int loadedCartL = 0, loadedProgL = 0; // what chip L actually holds
        int loadedCartR = 0, loadedProgR = 0; // what chip R actually holds
        float x = 0.5f, y = 0.5f, z = 0.5f;
        float blend = 0.5f;
        bool potQuantize = true;   // 9-bit pot ADC character switch

        // Convenience for tools/tests: hold what the panel shows.
        void loadInserted() noexcept
        {
            loadedCartL = loadedCartR = cartridge;
            loadedProgL = progL;
            loadedProgR = progR;
        }
    };

    void prepare(double hostRate, const Tolerances& tol)
    {
        fv1::CartridgeLibrary::ensureAssembled();

        const double skew = 1.0
            + tuning::kFv1ClockSkew * tol.bipolar(Tolerances::id("fx.clockR"));
        downL_.prepare(hostRate, fv1::Fv1Vm::kClockHz);
        upL_.prepare(fv1::Fv1Vm::kClockHz, hostRate);
        downR_.prepare(hostRate, fv1::Fv1Vm::kClockHz * skew);
        upR_.prepare(fv1::Fv1Vm::kClockHz * skew, hostRate);

        for (auto* s : { &xSm_, &ySm_, &zSm_ })
            s->prepare(hostRate, tuning::kFv1PotSmoothSec, 0.5f);
        blendSm_.prepare(hostRate, tuning::kBlendSmoothSec, params_.blend);

        heldCartL_ = params_.loadedCartL;
        heldProgL_ = params_.loadedProgL;
        heldCartR_ = params_.loadedCartR;
        heldProgR_ = params_.loadedProgR;
        vmL_.loadProgram(fv1::CartridgeLibrary::words(heldCartL_, heldProgL_));
        vmR_.loadProgram(fv1::CartridgeLibrary::words(heldCartR_, heldProgR_));
        fifoL_.reset();
        fifoR_.reset();
    }

    void setParams(const Params& p) noexcept
    {
        if (p.loadedCartL != heldCartL_ || p.loadedProgL != heldProgL_)
        {
            heldCartL_ = p.loadedCartL;
            heldProgL_ = p.loadedProgL;
            vmL_.loadProgram(fv1::CartridgeLibrary::words(heldCartL_, heldProgL_));
        }
        if (p.loadedCartR != heldCartR_ || p.loadedProgR != heldProgR_)
        {
            heldCartR_ = p.loadedCartR;
            heldProgR_ = p.loadedProgR;
            vmR_.loadProgram(fv1::CartridgeLibrary::words(heldCartR_, heldProgR_));
        }
        vmL_.setPotQuantize(p.potQuantize);
        vmR_.setPotQuantize(p.potQuantize);
        blendSm_.setTarget(p.blend);
        params_ = p;
    }

    // One host-rate sample, in volts (post DIST/GAIN). cvX/Y/Z are the panel
    // CV jacks in volts.
    void process(float& l, float& r, float cvX, float cvY, float cvZ) noexcept
    {
        xSm_.setTarget(std::clamp(params_.x + cvX * tuning::kFv1CvPerPot, 0.0f, 1.0f));
        ySm_.setTarget(std::clamp(params_.y + cvY * tuning::kFv1CvPerPot, 0.0f, 1.0f));
        zSm_.setTarget(std::clamp(params_.z + cvZ * tuning::kFv1CvPerPot, 0.0f, 1.0f));
        const float px = xSm_.next(), py = ySm_.next(), pz = zSm_.next();
        for (auto* vm : { &vmL_, &vmR_ })
        {
            vm->setPot(0, px);
            vm->setPot(1, py);
            vm->setPot(2, pz);
        }

        runChip(vmL_, downL_, upL_, fifoL_, l * kInScale);
        runChip(vmR_, downR_, upR_, fifoR_, r * kInScale);

        // Equal-power dry/wet; the dry leg is the analog path around the chips.
        const float b = blendSm_.next();
        const float wetGain = std::sin(1.5707963f * b);
        const float dryGain = std::cos(1.5707963f * b);
        l = dryGain * l + wetGain * fifoL_.pop() * tuning::kFv1FullScaleVolts;
        r = dryGain * r + wetGain * fifoR_.pop() * tuning::kFv1FullScaleVolts;
    }

private:
    static constexpr float kInScale = 1.0f / tuning::kFv1FullScaleVolts;

    // Tiny jitter buffer between the up-resampler and the host clock; the
    // long-run rate matches by construction, depth wobbles by a sample or two.
    struct Fifo
    {
        float buf[32] = {};
        uint32_t rd = 0, wr = 0;

        void reset() noexcept
        {
            rd = wr = 0;
            for (int i = 0; i < 4; ++i)
                push(0.0f); // prime to absorb resampler jitter
        }
        void push(float v) noexcept
        {
            if (wr - rd < 32)
                buf[wr++ & 31] = v;
        }
        float pop() noexcept { return rd < wr ? buf[rd++ & 31] : 0.0f; }
    };

    void runChip(fv1::Fv1Vm& vm, PolyphaseResampler& down, PolyphaseResampler& up,
                 Fifo& fifo, float in) noexcept
    {
        float chipIn[4], hostOut[4];
        const int n = down.push(in, chipIn, 4);
        for (int i = 0; i < n; ++i)
        {
            float wetL = 0.0f, wetR = 0.0f;
            vm.processSample(chipIn[i], chipIn[i], wetL, wetR);
            const int m = up.push(wetL, hostOut, 4);
            for (int j = 0; j < m; ++j)
                fifo.push(hostOut[j]);
        }
    }

    Params params_ {};
    int heldCartL_ = 0, heldProgL_ = 0; // what each VM currently holds
    int heldCartR_ = 0, heldProgR_ = 0;
    fv1::Fv1Vm vmL_, vmR_;
    PolyphaseResampler downL_, upL_, downR_, upR_;
    Fifo fifoL_, fifoR_;
    Smoother xSm_, ySm_, zSm_, blendSm_;
};

} // namespace s42
