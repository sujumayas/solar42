// fv1asm — CLI for the embedded SpinASM assembler.
// Assembles .spn -> 128-word FV-1 programs; output format matches asfv1's
// default binary (512 bytes, words big-endian) so the two can be diffed
// directly for the equivalence check.
#include "fv1/SpinAsm.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
    const char* inPath = nullptr;
    const char* outPath = nullptr;
    bool hexListing = false;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            outPath = argv[++i];
        else if (std::strcmp(argv[i], "--hex") == 0)
            hexListing = true;
        else if (argv[i][0] != '-')
            inPath = argv[i];
    }
    if (inPath == nullptr)
    {
        std::puts("usage: fv1asm <program.spn> [-o out.bin] [--hex]");
        return 2;
    }

    std::ifstream in(inPath);
    if (! in)
    {
        std::fprintf(stderr, "fv1asm: cannot open %s\n", inPath);
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();

    const auto res = s42::fv1::assembleSpinAsm(ss.str());
    for (const auto& w : res.warnings)
        std::fprintf(stderr, "warning: %s\n", w.c_str());
    for (const auto& e : res.errors)
        std::fprintf(stderr, "error: %s\n", e.c_str());
    if (! res.ok())
        return 1;

    std::fprintf(stderr, "fv1asm: %d instructions\n", res.instructionCount);

    if (hexListing)
        for (int i = 0; i < s42::fv1::Fv1Vm::kProgramWords; ++i)
            std::printf("%08X\n", res.words[(size_t) i]);

    if (outPath != nullptr)
    {
        std::ofstream out(outPath, std::ios::binary);
        for (const uint32_t w : res.words)
        {
            const unsigned char be[4] = { (unsigned char) (w >> 24), (unsigned char) (w >> 16),
                                          (unsigned char) (w >> 8), (unsigned char) w };
            out.write((const char*) be, 4);
        }
    }
    return 0;
}
