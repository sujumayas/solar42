#pragma once

#include <cstdint>

// MIDI real-time clock -> patchable pulse train (M9c P4). The processor
// collects this block's 0xF8 tick sample-offsets (plus START/CONTINUE/STOP)
// into a MidiClockFeed riding Rack::Controls; this source turns every Nth
// tick (the divider, default 6 = 1/16 notes at 24 ppqn) into a 10 V pulse
// on the MidiClockOut outlet — sample-accurate, patchable into any clock
// input like a hardware cable. Digital-only jack: no hardware counterpart.
//
// Run semantics: pulses flow whenever ticks arrive (free-running clocks
// never send START), START/CONTINUE resets the divider phase so bar-syncs
// land, STOP mutes the train until ticks are accompanied by a new
// START/CONTINUE. Pulse width is half the last divided interval (50 % duty;
// every Schmitt downstream trips at 2.5 V), first pulse defaults to 10 ms.
namespace s42 {

struct MidiClockFeed
{
    static constexpr int kMaxTicks = 64; // 24 ppqn at 300 BPM = 120 Hz — 64 covers any host block
    int32_t tickAt[kMaxTicks] = {};      // ascending sample offsets within the host block
    int numTicks = 0;
    bool start = false; // START (0xFA) or CONTINUE (0xFB) seen this block
    bool stop = false;  // STOP (0xFC) seen this block
};

class MidiClockSource
{
public:
    static constexpr float kPulseVolts = 10.0f; // gate convention (TouchKeyboard::kGateVolts)

    void prepare(double sampleRate) noexcept
    {
        sr_ = sampleRate;
        defaultWidth_ = (int) (sampleRate * 0.010);
        running_ = true;
        tickInDiv_ = 0;
        sincePulse_ = 0;
        pulseRemain_ = 0;
        haveInterval_ = false;
    }

    void setDivider(int ticks) noexcept { divTicks_ = ticks < 1 ? 1 : ticks; }

    // Once per host block, before process() calls (offsets are block-relative).
    void beginBlock(const MidiClockFeed& f) noexcept
    {
        feed_ = f;
        cursor_ = 0;
        if (f.stop)
        {
            running_ = false;
            pulseRemain_ = 0; // STOP silences immediately, tail included
        }
        if (f.start)
        {
            running_ = true;
            tickInDiv_ = 0; // phase reset: the next tick is a pulse
        }
    }

    // One sub-block: n samples starting at blockOffset within the host block.
    void process(float* out, int blockOffset, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int32_t pos = blockOffset + i;
            while (cursor_ < feed_.numTicks && feed_.tickAt[cursor_] <= pos)
            {
                ++cursor_;
                onTick();
            }
            out[i] = pulseRemain_ > 0 ? kPulseVolts : 0.0f;
            if (pulseRemain_ > 0)
                --pulseRemain_;
            ++sincePulse_;
        }
    }

private:
    void onTick() noexcept
    {
        if (!running_)
            return;
        if (tickInDiv_ == 0)
        {
            // 50 % duty of the divided interval, measured pulse-to-pulse.
            pulseRemain_ = haveInterval_ ? sincePulse_ / 2 : defaultWidth_;
            if (pulseRemain_ < 1)
                pulseRemain_ = 1;
            haveInterval_ = true;
            sincePulse_ = 0;
        }
        if (++tickInDiv_ >= divTicks_)
            tickInDiv_ = 0;
    }

    double sr_ = 48000.0;
    MidiClockFeed feed_ {};
    int cursor_ = 0;
    int divTicks_ = 6;
    int tickInDiv_ = 0;
    int32_t sincePulse_ = 0;
    int32_t pulseRemain_ = 0;
    int defaultWidth_ = 480;
    bool haveInterval_ = false;
    bool running_ = true; // free-running clocks never send START
};

} // namespace s42
