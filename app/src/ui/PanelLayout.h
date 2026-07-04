#pragma once

// Panel geometry in the logical 4950 x 3200 coordinate space (the hardware
// panel is 49.5 x 32 cm; reference-docs/solar42n-panel-1.png is the source).
// Phase 1 (M3) pins down the SECTION rectangles, measured off the render at
// 2.54 logical units per reference pixel; the per-control table (every knob,
// jack and switch position for the print layer + CableLayer) lands in M5.
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

// Performance zone (touch keyboard + keypad, M6). Debug UIs live here for now.
inline constexpr Rect kBottom { 36, 2160, 4884, 1004 };

} // namespace solar::layout
