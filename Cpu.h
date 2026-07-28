#pragma once

#include "common.h"
#include "Memoire.h"
//#include "Emulateur.h"
#include <array>

constexpr int RA = 31;  // registre "return address"

class CPU {
    std::array<u32, 32> regs{};
    std::array<u32, 32> cop0_regs{};

    bool cacheIsolated() const { return (cop0_regs[12] & 0x10000) != 0; }
    // Schema pc/next_pc pour les branch delay slots :
    // au moment d'executer une instruction ($),
    // pc = $+4 (delay slot) et next_pc = $+8.
    u32 current_pc = 0;
    u32 pc = 0xBFC00000;
    u32 next_pc = 0xBFC00004;

    // Registres speciaux multiplication/division
    u32 hi = 0;
    u32 lo = 0;

    u32 load_reg = 0;      // quel registre (0 = aucun, car r0 ignore les writes)
    u32 load_value = 0;

    Memoire& memoire;

public:
    CPU(Memoire& mem) : memoire(mem) {}
    u32 getPc() const { return pc; }
    u32 getReg(int i) const { return regs[i]; }
    void step();

    enum ExceptionCause {
        EXC_INT = 0x00,  // Interruption (materielle : VBlank, etc.)
        EXC_SYSCALL = 0x08,  // Instruction SYSCALL
        EXC_BREAK = 0x09,  // Instruction BREAK
        EXC_ILLEGAL = 0x0A,  // Instruction inconnue (Reserved Instruction)
        EXC_OVERFLOW = 0x0C,  // Overflow arithmetique (ADD/ADDI)
        // (il en existe d'autres : adresses mal alignees, etc. plus tard)
    };

    // Registres COP0 nommes (indices dans cop0_regs[])
    static constexpr int COP0_SR = 12;  // Status Register
    static constexpr int COP0_CAUSE = 13;  // Cause Register
    static constexpr int COP0_EPC = 14;  // Exception PC

    // Declenche une exception : sauve l'etat, saute au vecteur.
    void exception(u32 cause);

    // Verifie si une interruption materielle doit etre prise, et la prend.
    void checkInterrupts();
};

