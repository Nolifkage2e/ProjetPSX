#pragma once

#include "common.h"
#include "Memoire.h"
#include <array>

constexpr int RA = 31;  // registre "return address"

class CPU {
    std::array<u32, 32> regs{};
    std::array<u32, 32> cop0_regs{};

    bool cacheIsolated() const { return (cop0_regs[12] & 0x10000) != 0; }
    // Schema pc/next_pc pour les branch delay slots :
    // au moment d'executer une instruction ($),
    // pc = $+4 (delay slot) et next_pc = $+8.
    u32 pc = 0xBFC00000;
    u32 next_pc = 0xBFC00004;

    // Registres speciaux multiplication/division
    u32 hi = 0;
    u32 lo = 0;

    Memoire& memoire;

public:
    CPU(Memoire& mem) : memoire(mem) {}
    u32 getPc() const { return pc; }
    u32 getReg(int i) const { return regs[i]; }
    void step();
};