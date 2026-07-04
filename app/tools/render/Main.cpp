// solar42n_render — headless rig-state -> WAV regression renderer.
// Full version (load saved rig state, render N seconds, write WAV) lands with
// the regression harness; for now it proves the engine links and runs headless.
#include "engine/Rack.h"

#include <cstdio>
#include <vector>

int main()
{
    s42::Rack rack;
    rack.prepare(48000.0, 512);

    std::vector<float> l(48000), r(48000);
    for (int block = 0; block < 48000 / 512; ++block)
        rack.process(l.data() + block * 512, r.data() + block * 512, nullptr, nullptr, 512);

    std::puts("solar42n_render: rendered 1 s headless (silent M0 rack). "
              "State loading + WAV output arrive with the regression harness.");
    return 0;
}
