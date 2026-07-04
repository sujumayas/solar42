// fv1asm — CLI for the embedded SpinASM assembler (lands in M4).
// Will assemble .spn -> 128-word FV-1 programs and cross-check binaries
// against reference assemblers (asfv1 / SpinASM IDE).
#include <cstdio>

int main(int argc, char**)
{
    if (argc < 2)
    {
        std::puts("fv1asm (Solar 42N) — assembler arrives in milestone M4.");
        std::puts("usage: fv1asm <program.spn> [-o out.bin]");
        return 0;
    }
    std::fputs("fv1asm: not implemented yet (M4)\n", stderr);
    return 1;
}
