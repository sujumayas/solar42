#pragma once

#include "engine/Jacks.h"

// Panel geometry in the logical 4950 x 3200 coordinate space (the hardware
// panel is 49.5 x 32 cm, so 1 unit = 0.1 mm; reference-docs/solar42n-panel-1.png
// is the source, measured at 2.54 logical units per reference pixel).
//
// M3 pinned the SECTION rectangles; M5 completes the census: one entry here
// for every panel jack in the registry (engine/Jacks.h). The static print
// layer, the CableLayer's hit tests and the cable endpoints all read this one
// table. JUCE-free on purpose — the census is unit-tested.
namespace solar::layout {

struct Rect
{
    int x, y, w, h;
};

inline constexpr int kPanelW = 4950;
inline constexpr int kPanelH = 3200;

inline constexpr Rect kHeader { 0, 0, 4950, 360 };

// Row 1: classic drones flank the effector.
inline constexpr Rect kDrone1 { 36, 521, 808, 635 };
inline constexpr Rect kDrone2 { 858, 521, 808, 635 };
inline constexpr Rect kEffector { 1669, 368, 1613, 495 };
inline constexpr Rect kDrone4 { 3292, 521, 808, 635 };
inline constexpr Rect kDrone5 { 4115, 521, 805, 635 };

// Center column: filter strip, mixer, envelopes.
inline constexpr Rect kFilter { 1669, 863, 1613, 240 };
inline constexpr Rect kMixer { 1664, 1110, 1618, 350 };
inline constexpr Rect kEnvA { 1664, 1468, 711, 340 };
inline constexpr Rect kEnvB { 2565, 1468, 716, 340 };

// Row 2: srapa + VCO pairs.
inline constexpr Rect kDrone3 { 36, 1163, 808, 645 };
inline constexpr Rect kVcoA { 858, 1163, 808, 645 };
inline constexpr Rect kVcoB { 3292, 1163, 808, 645 };
inline constexpr Rect kDrone6 { 4115, 1163, 805, 645 };

// Red modulation strip.
inline constexpr Rect kLfoA { 36, 1819, 569, 302 };
inline constexpr Rect kJoystick { 615, 1819, 604, 302 };
inline constexpr Rect kSeq { 1237, 1819, 1951, 302 };
inline constexpr Rect kPreamp { 3206, 1819, 351, 302 };
inline constexpr Rect kEnvFollower { 3566, 1819, 767, 302 };
inline constexpr Rect kLfoB { 4343, 1819, 577, 302 };

// Performance zone (measured off the render's bottom band).
inline constexpr Rect kBottom { 36, 2160, 4884, 1004 };
inline constexpr Rect kJoyPad { 150, 2380, 500, 560 };    // physical stick (joy.x/joy.y)
inline constexpr Rect kKeyboard { 838, 2286, 2235, 800 }; // black touch-plate panel (M6)
inline constexpr Rect kKeypad { 4390, 2330, 470, 760 };   // DRONE VOICES 2x3 keypad

// Digital-only environment controls live in the header (no silk-screened home).
inline constexpr Rect kRoomLight { 4020, 80, 300, 250 };
inline constexpr Rect kFlicker { 4330, 130, 170, 150 };

// ------------------------------------------------------------------- jacks

// Visual radii (1 unit = 0.1 mm): 6.35 mm plug hole in an ~11 mm hex nut.
inline constexpr int kJackR = 33;    // ring radius for the print layer
inline constexpr int kJackHitR = 90; // hit radius (>= 24 px at fit-width; nearest wins)

enum : int8_t { kLabelBelow = 0, kLabelAbove = 1, kLabelLeft = 2 };

struct Jack
{
    bool isInlet;
    int16_t index; // s42::Inlet or s42::Outlet
    int x, y;
    const char* label;
    int8_t labelSide; // kLabelBelow default; Above/Left where the print does
};

constexpr int jx(const Rect& r, double fx) noexcept { return r.x + (int) (fx * r.w + 0.5); }
constexpr int jy(const Rect& r, double fy) noexcept { return r.y + (int) (fy * r.h + 0.5); }

constexpr Jack in(s42::Inlet i, int x, int y, const char* label, int8_t side = kLabelBelow) noexcept
{
    return { true, (int16_t) i, x, y, label, side };
}
constexpr Jack out(s42::Outlet o, int x, int y, const char* label, int8_t side = kLabelBelow) noexcept
{
    return { false, (int16_t) o, x, y, label, side };
}

using s42::Inlet;
using s42::Outlet;

inline constexpr Jack kJacks[] = {
    // Classic drone voices (bottom rows: GATE . HOLD . ATT . RLS . CV . env).
    in(Inlet::D1GateIn, jx(kDrone1, 0.075), jy(kDrone1, 0.855), "GATE"),
    in(Inlet::D1CvIn, jx(kDrone1, 0.535), jy(kDrone1, 0.855), "CV"),
    out(Outlet::D1EnvOut, jx(kDrone1, 0.655), jy(kDrone1, 0.855), "env"),
    in(Inlet::D2GateIn, jx(kDrone2, 0.075), jy(kDrone2, 0.855), "GATE"),
    in(Inlet::D2CvIn, jx(kDrone2, 0.535), jy(kDrone2, 0.855), "CV"),
    out(Outlet::D2EnvOut, jx(kDrone2, 0.655), jy(kDrone2, 0.855), "env"),
    in(Inlet::D4GateIn, jx(kDrone4, 0.075), jy(kDrone4, 0.855), "GATE"),
    in(Inlet::D4CvIn, jx(kDrone4, 0.535), jy(kDrone4, 0.855), "CV"),
    out(Outlet::D4EnvOut, jx(kDrone4, 0.655), jy(kDrone4, 0.855), "env"),
    in(Inlet::D5GateIn, jx(kDrone5, 0.075), jy(kDrone5, 0.855), "GATE"),
    in(Inlet::D5CvIn, jx(kDrone5, 0.535), jy(kDrone5, 0.855), "CV"),
    out(Outlet::D5EnvOut, jx(kDrone5, 0.655), jy(kDrone5, 0.855), "env"),

    // Papa Srapa voices (cv out mid-right; GATE . env . S&H in/clock/out row).
    out(Outlet::D3CvOut, jx(kDrone3, 0.615), jy(kDrone3, 0.545), "cv"),
    in(Inlet::D3GateIn, jx(kDrone3, 0.075), jy(kDrone3, 0.855), "GATE"),
    out(Outlet::D3EnvOut, jx(kDrone3, 0.435), jy(kDrone3, 0.855), "env out"),
    in(Inlet::D3ShIn, jx(kDrone3, 0.635), jy(kDrone3, 0.855), "in"),
    in(Inlet::D3ShClockIn, jx(kDrone3, 0.76), jy(kDrone3, 0.855), "clock"),
    out(Outlet::D3ShOut, jx(kDrone3, 0.885), jy(kDrone3, 0.855), "out"),
    out(Outlet::D6CvOut, jx(kDrone6, 0.615), jy(kDrone6, 0.545), "cv"),
    in(Inlet::D6GateIn, jx(kDrone6, 0.075), jy(kDrone6, 0.855), "GATE"),
    out(Outlet::D6EnvOut, jx(kDrone6, 0.435), jy(kDrone6, 0.855), "env out"),
    in(Inlet::D6ShIn, jx(kDrone6, 0.635), jy(kDrone6, 0.855), "in"),
    in(Inlet::D6ShClockIn, jx(kDrone6, 0.76), jy(kDrone6, 0.855), "clock"),
    out(Outlet::D6ShOut, jx(kDrone6, 0.885), jy(kDrone6, 0.855), "out"),

    // VCO A/B bottom jack rows (42N: A has sync in, only B has an osc out).
    in(Inlet::VcoAVoctIn, jx(kVcoA, 0.14), jy(kVcoA, 0.865), "1v/oct"),
    in(Inlet::VcoACvIn, jx(kVcoA, 0.385), jy(kVcoA, 0.865), "cv"),
    in(Inlet::VcoAPwmIn, jx(kVcoA, 0.625), jy(kVcoA, 0.865), "pwm"),
    in(Inlet::VcoASyncIn, jx(kVcoA, 0.865), jy(kVcoA, 0.865), "sync"),
    in(Inlet::VcoBVoctIn, jx(kVcoB, 0.14), jy(kVcoB, 0.865), "1v/oct"),
    in(Inlet::VcoBCvIn, jx(kVcoB, 0.385), jy(kVcoB, 0.865), "cv"),
    in(Inlet::VcoBPwmIn, jx(kVcoB, 0.625), jy(kVcoB, 0.865), "pwm"),
    out(Outlet::VcoBOscOut, jx(kVcoB, 0.865), jy(kVcoB, 0.865), "osc"),

    // Envelope A/B jack rows; the red VCO A/B DRY OUTs flank the mixer logo.
    // Row raised to fy 0.72 (M9b P1.5) so the labels fit BELOW the nuts like
    // every other row on the hardware print.
    in(Inlet::EnvAGateIn, jx(kEnvA, 0.16), jy(kEnvA, 0.72), "gate"),
    out(Outlet::EnvAOut, jx(kEnvA, 0.40), jy(kEnvA, 0.72), "env"),
    in(Inlet::EnvAVcaCvIn, jx(kEnvA, 0.64), jy(kEnvA, 0.72), "vca cv"),
    out(Outlet::VcoADryOut, jx(kEnvA, 0.88), jy(kEnvA, 0.72), "VCO A"),
    out(Outlet::VcoBDryOut, jx(kEnvB, 0.12), jy(kEnvB, 0.72), "VCO B"),
    in(Inlet::EnvBGateIn, jx(kEnvB, 0.36), jy(kEnvB, 0.72), "gate"),
    out(Outlet::EnvBOut, jx(kEnvB, 0.60), jy(kEnvB, 0.72), "env"),
    in(Inlet::EnvBVcaCvIn, jx(kEnvB, 0.84), jy(kEnvB, 0.72), "vca cv"),

    // Filter strip (13 columns; CV L / CV R sit between the knobs).
    in(Inlet::FiltCvLIn, jx(kFilter, 4.5 / 13.0), jy(kFilter, 0.60), "CV L"),
    in(Inlet::FiltCvRIn, jx(kFilter, 8.5 / 13.0), jy(kFilter, 0.60), "CV R"),

    // Effector CV jacks (top-left row, above the X/Y/Z knobs).
    in(Inlet::FxCvXIn, jx(kEffector, 0.045), jy(kEffector, 0.33), "cv x"),
    in(Inlet::FxCvYIn, jx(kEffector, 0.125), jy(kEffector, 0.33), "cv y"),
    in(Inlet::FxCvZIn, jx(kEffector, 0.205), jy(kEffector, 0.33), "cv z"),

    // Modulation strip.
    out(Outlet::LfoAOut, jx(kLfoA, 0.44), jy(kLfoA, 0.42), "out"),
    out(Outlet::JoyXOut, jx(kJoystick, 0.42), jy(kJoystick, 0.42), "X"),
    out(Outlet::JoyYOut, jx(kJoystick, 0.60), jy(kJoystick, 0.42), "Y"),
    in(Inlet::SeqClockIn, jx(kSeq, 0.115), jy(kSeq, 0.42), "clock"),
    // cv over gate stacked at the strip's right edge like the print; the
    // clock out keeps its census jack even though the render art hides it.
    out(Outlet::SeqClockOut, jx(kSeq, 0.83), jy(kSeq, 0.42), "clock"),
    out(Outlet::SeqCvOut, jx(kSeq, 0.945), jy(kSeq, 0.30), "cv", kLabelLeft),
    out(Outlet::SeqGateOut, jx(kSeq, 0.945), jy(kSeq, 0.66), "gate", kLabelLeft),
    in(Inlet::PreampExtIn, jx(kPreamp, 0.27), jy(kPreamp, 0.42), "ext. source"),
    out(Outlet::EnvFEnvOut, jx(kEnvFollower, 0.64), jy(kEnvFollower, 0.42), "env"),
    out(Outlet::EnvFGateOut, jx(kEnvFollower, 0.875), jy(kEnvFollower, 0.42), "gate"),
    out(Outlet::LfoBOut, jx(kLfoB, 0.44), jy(kLfoB, 0.42), "out"),

    // Header (EXT AUDIO feeds the mixer channel; WET OUT is print-only).
    in(Inlet::ExtAudioIn, 340, 110, "EXT. AUDIO"),

    // Touch keyboard jack strip (registry jacks live from M2; the keyboard
    // module lands in M6 — outs read 0 V until then). The render hides these
    // (rear panel on hardware); parked right of the keyboard, above the zone.
    out(Outlet::KbVoctOut, 3220, 2255, "V/OCT"),
    out(Outlet::KbGateLOut, 3390, 2255, "GATE L"),
    out(Outlet::KbGateROut, 3560, 2255, "GATE R"),
    out(Outlet::KbPressOut, 3730, 2255, "PRESS"),
    in(Inlet::KbClockIn, 3900, 2255, "CLOCK"),
    in(Inlet::KbResetIn, 4070, 2255, "RESET"),
};

inline constexpr int kNumJacks = (int) (sizeof(kJacks) / sizeof(kJacks[0]));

// The census must stay complete: every inlet and every non-hidden outlet has
// exactly one panel position (checked here so a registry append without a
// layout entry fails to compile; geometry itself is unit-tested).
constexpr bool jackTableCovers() noexcept
{
    int inletHits[s42::kInletCount] = {};
    int outletHits[s42::kOutletCount] = {};
    for (const auto& j : kJacks)
        ++(j.isInlet ? inletHits[j.index] : outletHits[j.index]);
    for (int i = 0; i < s42::kInletCount; ++i)
        if (inletHits[i] != 1)
            return false;
    for (int o = 0; o < s42::kOutletCount; ++o)
        if (outletHits[o] != (s42::kOutletInfo[o].hidden ? 0 : 1))
            return false;
    return true;
}
static_assert(jackTableCovers(), "PanelLayout jack table out of sync with engine/Jacks.h");

} // namespace solar::layout
