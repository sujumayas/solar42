#pragma once

#include <cstdint>

// The single source of truth for every patch point on the Solar 42N panel
// (07-42n-panel-inventory.md §2-3). Engine routing, the cable UI, and state
// serialization all consume these tables — the panel's ~50 jacks stay in sync
// by construction. 42N voice naming: DRONE 1,2,4,5 classic; DRONE 3,6 Papa
// Srapa; VCO A/B.
//
// Modules that don't exist yet simply never write their outlets (which then
// read 0 V) — the census is complete from M2 so jack ids never change.

namespace s42 {

// X(id, displayName, hidden)
// "hidden" outlets exist for normalling but are not panel jacks (VCO A's raw
// output on the 42N — only VCO B has a panel `osc` out).
#define S42_OUTLETS(X) \
    X(D1EnvOut,   "drone1.env.out",   false) \
    X(D2EnvOut,   "drone2.env.out",   false) \
    X(D4EnvOut,   "drone4.env.out",   false) \
    X(D5EnvOut,   "drone5.env.out",   false) \
    X(D3CvOut,    "drone3.cv.out",    false) \
    X(D3EnvOut,   "drone3.env.out",   false) \
    X(D3ShOut,    "drone3.sh.out",    false) \
    X(D6CvOut,    "drone6.cv.out",    false) \
    X(D6EnvOut,   "drone6.env.out",   false) \
    X(D6ShOut,    "drone6.sh.out",    false) \
    X(VcoAOscInt, "vcoA.osc.internal", true) \
    X(VcoBOscOut, "vcoB.osc.out",     false) \
    X(EnvAOut,    "envA.out",         false) \
    X(EnvBOut,    "envB.out",         false) \
    X(LfoAOut,    "lfoA.out",         false) \
    X(LfoBOut,    "lfoB.out",         false) \
    X(JoyXOut,    "joy.x.out",        false) \
    X(JoyYOut,    "joy.y.out",        false) \
    X(SeqClockOut,"seq.clock.out",    false) \
    X(SeqCvOut,   "seq.cv.out",       false) \
    X(SeqGateOut, "seq.gate.out",     false) \
    X(EnvFEnvOut, "envf.env.out",     false) \
    X(EnvFGateOut,"envf.gate.out",    false) \
    X(KbVoctOut,  "kb.voct.out",      false) \
    X(KbGateLOut, "kb.gateL.out",     false) \
    X(KbGateROut, "kb.gateR.out",     false) \
    X(KbPressOut, "kb.press.out",     false)

// Normal kinds: what an unpatched inlet reads.
//   None       -> 0 V
//   FromOutlet -> the given outlet (hardware normalled connection)
//   FollowCvL  -> whatever is patched into filter CV L (42N: CV L -> CV R)
//   Internal   -> module supplies its own source (Papa Srapa noise -> S&H in)
//   HostInput  -> the plugin's external audio input (EXT AUDIO)
// X(id, displayName, normalKind, normalOutlet)
#define S42_INLETS(X) \
    X(D1GateIn,    "drone1.gate.in",   None,       LfoAOut) \
    X(D1CvIn,      "drone1.cv.in",     None,       LfoAOut) \
    X(D2GateIn,    "drone2.gate.in",   None,       LfoAOut) \
    X(D2CvIn,      "drone2.cv.in",     None,       LfoAOut) \
    X(D4GateIn,    "drone4.gate.in",   None,       LfoAOut) \
    X(D4CvIn,      "drone4.cv.in",     None,       LfoAOut) \
    X(D5GateIn,    "drone5.gate.in",   None,       LfoAOut) \
    X(D5CvIn,      "drone5.cv.in",     None,       LfoAOut) \
    X(D3GateIn,    "drone3.gate.in",   None,       LfoAOut) \
    X(D3ShIn,      "drone3.sh.in",     Internal,   LfoAOut) \
    X(D3ShClockIn, "drone3.sh.clock.in", None,     LfoAOut) \
    X(D6GateIn,    "drone6.gate.in",   None,       LfoAOut) \
    X(D6ShIn,      "drone6.sh.in",     Internal,   LfoAOut) \
    X(D6ShClockIn, "drone6.sh.clock.in", None,     LfoAOut) \
    X(VcoAVoctIn,  "vcoA.voct.in",     FromOutlet, KbVoctOut) \
    X(VcoACvIn,    "vcoA.cv.in",       None,       LfoAOut) \
    X(VcoAPwmIn,   "vcoA.pwm.in",      None,       LfoAOut) \
    X(VcoASyncIn,  "vcoA.sync.in",     None,       LfoAOut) \
    X(VcoBVoctIn,  "vcoB.voct.in",     FromOutlet, KbVoctOut) \
    X(VcoBCvIn,    "vcoB.cv.in",       FromOutlet, VcoAOscInt) \
    X(VcoBPwmIn,   "vcoB.pwm.in",      None,       LfoAOut) \
    X(EnvAGateIn,  "envA.gate.in",     FromOutlet, KbGateLOut) \
    X(EnvAVcaCvIn, "envA.vcacv.in",    None,       LfoAOut) \
    X(EnvBGateIn,  "envB.gate.in",     FromOutlet, KbGateLOut) \
    X(EnvBVcaCvIn, "envB.vcacv.in",    None,       LfoAOut) \
    X(FiltCvLIn,   "filt.cvl.in",      None,       LfoAOut) \
    X(FiltCvRIn,   "filt.cvr.in",      FollowCvL,  LfoAOut) \
    X(FxCvXIn,     "fx.cvx.in",        None,       LfoAOut) \
    X(FxCvYIn,     "fx.cvy.in",        None,       LfoAOut) \
    X(FxCvZIn,     "fx.cvz.in",        None,       LfoAOut) \
    X(SeqClockIn,  "seq.clock.in",     None,       LfoAOut) \
    X(KbClockIn,   "kb.clock.in",      None,       LfoAOut) \
    X(KbResetIn,   "kb.reset.in",      None,       LfoAOut) \
    X(ExtAudioIn,  "ext.audio.in",     HostInput,  LfoAOut)

enum class Outlet : int16_t
{
#define X(id, name, hidden) id,
    S42_OUTLETS(X)
#undef X
    Count
};

enum class Inlet : int16_t
{
#define X(id, name, kind, out) id,
    S42_INLETS(X)
#undef X
    Count
};

inline constexpr int kOutletCount = (int) Outlet::Count;
inline constexpr int kInletCount = (int) Inlet::Count;

enum class NormalKind : uint8_t { None, FromOutlet, FollowCvL, Internal, HostInput };

struct InletInfo
{
    const char* name;
    NormalKind kind;
    Outlet normalOutlet; // meaningful only for FromOutlet
};

struct OutletInfo
{
    const char* name;
    bool hidden;
};

inline constexpr OutletInfo kOutletInfo[] = {
#define X(id, name, hidden) { name, hidden },
    S42_OUTLETS(X)
#undef X
};

inline constexpr InletInfo kInletInfo[] = {
#define X(id, name, kind, out) { name, NormalKind::kind, Outlet::out },
    S42_INLETS(X)
#undef X
};

} // namespace s42
