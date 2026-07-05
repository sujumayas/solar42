#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Jacks.h"

// M7 conveniences: every jack explains itself from the registry (name,
// direction, voltage range, hardware normal), and the non-obvious panel
// controls carry curated hints. Obvious controls fall back to the parameter
// name so nothing shows an empty bubble.
namespace solar::tips {

// ---- voltage ranges per jack (07 §4 voltage table).
inline const char* outletRange(s42::Outlet o)
{
    using O = s42::Outlet;
    switch (o)
    {
        case O::D1EnvOut: case O::D2EnvOut: case O::D4EnvOut: case O::D5EnvOut:
        case O::D3EnvOut: case O::D6EnvOut: return "+-10 V envelope";
        case O::D3CvOut: case O::D6CvOut: return "0..+12 V modulator";
        case O::D3ShOut: case O::D6ShOut: return "+-5 V steps";
        case O::VcoBOscOut: return "+-5 V audio";
        case O::VcoADryOut: case O::VcoBDryOut: return "~1 V audio (pre-filter dry)";
        case O::EnvAOut: case O::EnvBOut: return "0..8 V envelope";
        case O::LfoAOut: case O::LfoBOut: return "0..+10 V (unipolar by design - it can drive the sensor LEDs)";
        case O::JoyXOut: case O::JoyYOut: return "+-10 V";
        case O::SeqClockOut: return "+-10 V pulse";
        case O::SeqCvOut: return "0..5 V steps";
        case O::SeqGateOut: case O::EnvFGateOut: return "0/8 V gate";
        case O::EnvFEnvOut: return "0..10 V follower";
        case O::KbVoctOut: return "0..8 V, 1 V/oct";
        case O::KbGateLOut: case O::KbGateROut: return "0/8 V gate";
        case O::KbPressOut: return "0..8 V pressure";
        default: return "";
    }
}

inline const char* inletHint(s42::Inlet i)
{
    using I = s42::Inlet;
    switch (i)
    {
        case I::D1GateIn: case I::D2GateIn: case I::D4GateIn: case I::D5GateIn:
        case I::D3GateIn: case I::D6GateIn:
            return "Gate: fires the voice envelope above 2.5 V (keypad button in parallel)";
        case I::D1CvIn: case I::D2CvIn: case I::D4CvIn: case I::D5CvIn:
            return "Drives this voice's photo-sensor LED - generators with MOD down follow the light";
        case I::D3ShIn: case I::D6ShIn:
            return "Sample & hold input";
        case I::D3ShClockIn: case I::D6ShClockIn:
            return "S&H clock: samples on the rising edge";
        case I::VcoAVoctIn: case I::VcoBVoctIn:
            return "Pitch, 1 V/oct";
        case I::VcoACvIn: case I::VcoBCvIn:
            return "FM input; CV AMT sets depth, lin/exp switch picks the law";
        case I::VcoAPwmIn: case I::VcoBPwmIn:
            return "Pulse-width modulation for the square wave";
        case I::VcoASyncIn:
            return "Hard sync: resets VCO A's core on each rising edge";
        case I::EnvAGateIn: case I::EnvBGateIn:
            return "Envelope gate (above 2.5 V)";
        case I::EnvAVcaCvIn: case I::EnvBVcaCvIn:
            return "Adds straight onto the VCA control voltage";
        case I::FiltCvLIn: case I::FiltCvRIn:
            return "Filter cutoff CV - the MOD knob sets depth (1 V/oct at full)";
        case I::FxCvXIn: case I::FxCvYIn: case I::FxCvZIn:
            return "Adds CV/20 to the matching effector knob";
        case I::SeqClockIn:
            return "External sequencer clock - overrides the pulser while patched";
        case I::KbClockIn:
            return "External keyboard clock (arp/seq) - the firmware auto-switches while pulses arrive";
        case I::KbResetIn:
            return "Returns arp/seq to step 1 on the rising edge";
        case I::ExtAudioIn:
            return "EXT AUDIO mixer channel (+-5 V line level)";
        case I::PreampExtIn:
            return "Preamp source - replaces the host/mic input while patched";
        default: return "";
    }
}

// Full tooltip for a panel jack, from the frozen registry.
inline juce::String jackTooltip(bool isInlet, int index)
{
    juce::String t;
    if (isInlet)
    {
        const auto& info = s42::kInletInfo[index];
        t << info.name << "  -  INPUT";
        const auto* hint = inletHint((s42::Inlet) index);
        if (hint[0] != '\0')
            t << "\n" << hint;
        switch (info.kind)
        {
            case s42::NormalKind::FromOutlet:
                t << "\nNormalled from " << s42::kOutletInfo[(int) info.normalOutlet].name
                  << " - patching a cable breaks the normal.";
                break;
            case s42::NormalKind::FollowCvL:
                t << "\nNormalled to whatever CV L reads (42N trait) - patching breaks it.";
                break;
            case s42::NormalKind::Internal:
                t << "\nNormalled to this voice's own noise - patching breaks it.";
                break;
            case s42::NormalKind::HostInput:
                t << "\nFed by the plugin's audio input while unpatched.";
                break;
            case s42::NormalKind::None:
                break;
        }
    }
    else
    {
        const auto& info = s42::kOutletInfo[index];
        t << info.name << "  -  OUTPUT";
        const auto* range = outletRange((s42::Outlet) index);
        if (range[0] != '\0')
            t << "  ·  " << range;
        t << "\nDrag to any input jack; one output feeds any number of cables (built-in mult).";
    }
    return t;
}

// ---- curated control hints. Exact ids first, then per-voice suffix rules;
// fall back to the parameter's display name.
inline juce::String controlTooltip(const juce::String& id, const juce::String& paramName)
{
    // Exact ids (shared / one-of controls).
    struct Exact { const char* id; const char* tip; };
    static constexpr Exact exact[] = {
        { "filt.dist", "Double distortion: crossfades the clean path against two cascaded "
                       "asymmetric waveshapers (GAIN drives them). Shared by both filters." },
        { "filt.gain", "Drive into the double-distortion stages - from warm push to full snarl." },
        { "filt.link", "LINK: filter L's CV product controls both channels." },
        { "filt.modL", "How much of the CV L jack reaches filter L's cutoff (1 V/oct at full)." },
        { "filt.modR", "How much of the CV R jack reaches filter R's cutoff. CV R follows CV L "
                       "until something is patched into it (42N normal)." },
        { "fx.cart", "Cartridge slot. Inserting a cartridge changes NOTHING until a channel's "
                     "1-2-3 toggle flips - each chip keeps playing what it holds (hardware-true)." },
        { "fx.progL", "Loads program 1/2/3 from the inserted cartridge into chip L "
                      "(RAM cleared, LFOs re-jammed - the tail cuts, like hardware)." },
        { "fx.progR", "Loads program 1/2/3 from the inserted cartridge into chip R." },
        { "fx.x", "Effector X - the inserted program's first macro. CV X adds CV/20." },
        { "fx.y", "Effector Y - second macro. CV Y adds CV/20." },
        { "fx.z", "Effector Z - third macro. CV Z adds CV/20." },
        { "fx.blend", "Equal-power blend: analog path around the chips vs the chip returns." },
        { "fx.potq", "9-bit pot ADCs with hysteresis, like the real FV-1 - a character zipper "
                     "on slow X/Y/Z sweeps. Off = smooth digital pots." },
        { "joy.x", "Joystick X - position-locking stick, +-10 V at the X jack." },
        { "joy.y", "Joystick Y - position-locking stick, +-10 V at the Y jack." },
        { "joy.xoff", "Shifts the X output window: left -10..0 V, centre -5..+5 V, right 0..+10 V." },
        { "joy.yoff", "Shifts the Y output window: left -10..0 V, centre -5..+5 V, right 0..+10 V." },
        { "seq.pulser", "Internal clock rate. The pulser is normalled to the sequencer clock; "
                        "patching seq.clock.in overrides it." },
        { "seq.stages", "How many steps the sequencer cycles (3/4/5)." },
        { "pre.gain", "Up to 40 dB into a soft rail clip - line, contact mic, or the internal piezo." },
        { "envf.att", "How fast the follower's envelope rises with the preamp signal." },
        { "envf.rel", "How slowly the envelope falls; the gate output switches at Schmitt thresholds." },
        { "room.light", "Ambient light on ALL photo-sensor windows (digital-only control). "
                        "Dark room = patched LEDs own the sensors; bright room pins them." },
        { "room.flicker", "Adds 100 Hz mains lamp ripple to the room light - vintage-bulb shimmer "
                          "in the sensors." },
        { "master.vol", "Master output. WET OUT calibration: 2 V peak = -6 dBFS." },
    };
    for (const auto& e : exact)
        if (id == e.id)
            return e.tip;

    // Per-voice / per-section rules.
    auto has = [&](const char* s) { return id.contains(s); };
    auto ends = [&](const char* s) { return id.endsWith(s); };

    if (has(".gen") && ends(".tune"))
        return "This generator's free-run pitch across its factory range. Neighbouring "
               "generators beat against it - the drone IS the beating.";
    if (has(".gen") && ends(".mute"))
        return "Silences this generator (its pitch keeps running underneath).";
    if (has(".gen") && ends(".mod"))
        return "MOD: this generator's pitch follows the voice's photo-sensor "
               "(room light + whatever the CV jack drives into the LED).";
    if (ends(".volt"))
        return "VOLT: starves the whole voice - first half transposes every generator "
               "down ~2.5 octaves, past halfway the generators cross-modulate (the dirty zone).";
    if (ends(".fmamt"))
        return "FM depth: how far the sub-audio modulator throws the audio oscillator "
               "(siren interval at full).";
    if (ends(".divider"))
        return "Integer flip-flop divider on the FM modulator - stepped sub-octaves of the rate.";
    if (ends(".x10"))
        return "Rate range x10.";
    if (ends(".hi"))
        return "Audio oscillator range: low ~C0.., hi ~C2..E7.";
    if (ends(".fm") && !ends(".fmamt"))
        return "FM: the sub-audio oscillator modulates the audio oscillator's pitch.";
    if (ends(".am"))
        return "AM: the modulator chops the audio oscillator (~30 us edges - soft click, no thump).";
    if (ends(".noise") && (id.startsWith("d3") || id.startsWith("d6")))
        return "Blends the noise source into the voice; the S&H input is normalled to it.";
    if (ends(".rate") && (id.startsWith("d3") || id.startsWith("d6")))
        return "Sub-audio modulator rate (RC relaxation core - never a clean 50 % square).";
    if (ends(".pitch") && (id.startsWith("d3") || id.startsWith("d6")))
        return "Audio oscillator pitch (~C0..E7 across hi/low).";
    if (ends(".hold"))
        return "HOLD: keeps the voice VCA open without a gate.";
    if (ends(".gate") && id.length() <= 8)
        return "Keypad gate for this voice (DRONE VOICES buttons in the performance zone).";
    if (ends(".att"))
        return "Envelope attack.";
    if (ends(".rls"))
        return "Envelope release.";
    if (ends(".cvamt"))
        return "FM depth from the cv jack (VCO B's cv is normalled to VCO A's core - "
               "turn this up for instant 2-op FM).";
    if (ends(".wave") && (id.startsWith("vco")))
        return "Morphing waveform: saw <-> inverted saw, then sine <-> triangle, then square "
               "(PW/PWM take over).";
    if (ends(".wave"))
        return "Waveshape morph: square -> trapezoid -> triangle (a true slew-limit morph).";
    if (ends(".pwm"))
        return "How much of the pwm jack reaches the pulse width.";
    if (ends(".pw"))
        return "Square-wave pulse width.";
    if (ends(".oct"))
        return "Octave switch: +3 octaves.";
    if (ends(".sub"))
        return "Adds the f/2 sub-square under the main wave.";
    if (ends(".exp"))
        return "CV law for the cv jack: linear (through the 3340's non-through-zero FM) or "
               "exponential.";
    if (ends(".loop"))
        return "LOOP: the envelope retriggers itself - envelope as LFO.";
    if (ends(".range"))
        return "Rate range multiplier (x1 / x6 / x10).";
    if (id.startsWith("mix.") && ends(".vol"))
        return "Channel level into the filter buses (audio taper).";
    if (id.startsWith("mix.") && ends(".pan"))
        return "PAN is the filter routing: it crossfades this channel between "
               "Filter L and Filter R (constant power).";
    if (id.startsWith("filt.freq"))
        return "Cutoff. Push RES past ~0.85 and the Polivoks sings - bounded, angry, "
               "and it never loses bass.";
    if (id.startsWith("filt.res"))
        return "Resonance; self-oscillation starts around 0.85 (slightly different L vs R - "
               "component tolerances).";
    if (id.startsWith("filt.bp"))
        return "BP/LP mode for this filter channel.";
    if (id.startsWith("seq.step"))
        return "This step's voltage (0..5 V at the cv out).";
    if (id.startsWith("seq.gate"))
        return "Whether this step fires the gate output.";
    if (ends(".rate"))
        return "LFO rate (see the range switch).";

    return paramName; // nothing curated - at least name it
}

} // namespace solar::tips
