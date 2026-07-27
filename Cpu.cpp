#include "Cpu.h"
#include <iostream>

void CPU::step() {
    u32 instr = memoire.load32(pc);
    pc = next_pc;       // pc pointe maintenant sur le delay slot ($+4)
    next_pc += 4;       // next_pc pointe sur $+8

    // Decodage des champs
    u32 opcode = instr >> 26;
    u32 rs = (instr >> 21) & 0x1F;
    u32 rt = (instr >> 16) & 0x1F;
    u32 rd = (instr >> 11) & 0x1F;
    u32 shamt = (instr >> 6) & 0x1F;   // pour SLL/SRL/SRA
    u32 imm = instr & 0xFFFF;         // immediat 16 bits
    u32 imm26 = instr & 0x03FFFFFF;     // cible pour J/JAL

    // Destination des branchements : $+4 + offset signe * 4
    // (pc vaut deja $+4 ici, donc dest = pc + offset<<2)
    u32 branch_dest = pc + ((s16)imm << 2);

    switch (opcode) {
    case 0x00: { // R-type
        u32 funct = instr & 0x3F;
        switch (funct) {

        case 0x00: // SLL (nop si instr == 0)
            regs[rd] = regs[rt] << shamt;
            break;
        case 0x02: // SRL (logique : remplit avec des 0)
            regs[rd] = regs[rt] >> shamt;
            break;
        case 0x03: // SRA (arithmetique : recopie le bit de signe)
            regs[rd] = (s32)regs[rt] >> shamt;
            break;
        case 0x04: // SLLV (quantite dans rs, masquee sur 5 bits)
            regs[rd] = regs[rt] << (regs[rs] & 0x1F);
            break;
        case 0x06: // SRLV
            regs[rd] = regs[rt] >> (regs[rs] & 0x1F);
            break;
        case 0x07: // SRAV
            regs[rd] = (s32)regs[rt] >> (regs[rs] & 0x1F);
            break;

        case 0x08: // JR : le saut passe par next_pc (delay slot!)
            next_pc = regs[rs];
            break;
        case 0x09: // JALR : rd = $+8, saut vers rs
            regs[rd] = next_pc;
            next_pc = regs[rs];
            break;

        case 0x10: // MFHI
            regs[rd] = hi;
            break;
        case 0x11: // MTHI
            hi = regs[rs];
            break;
        case 0x12: // MFLO
            regs[rd] = lo;
            break;
        case 0x13: // MTLO
            lo = regs[rs];
            break;

        case 0x18: { // MULT (signe) : resultat 64 bits dans HI:LO
            s64 result = (s64)(s32)regs[rs] * (s64)(s32)regs[rt];
            hi = (u32)((u64)result >> 32);
            lo = (u32)result;
            break;
        }
        case 0x19: { // MULTU (non signe)
            u64 result = (u64)regs[rs] * (u64)regs[rt];
            hi = (u32)(result >> 32);
            lo = (u32)result;
            break;
        }
        case 0x1A: { // DIV (signe) : LO = quotient, HI = reste
            s32 n = (s32)regs[rs];
            s32 d = (s32)regs[rt];
            if (d == 0) {
                // Division par zero : pas d'exception sur MIPS,
                // valeurs "poubelle" definies par le hardware
                hi = (u32)n;
                lo = (n >= 0) ? 0xFFFFFFFF : 1;
            }
            else if ((u32)n == 0x80000000 && d == -1) {
                // Cas d'overflow : -2^31 / -1 ne tient pas sur 32 bits
                hi = 0;
                lo = 0x80000000;
            }
            else {
                lo = (u32)(n / d);
                hi = (u32)(n % d);
            }
            break;
        }
        case 0x1B: { // DIVU (non signe)
            u32 n = regs[rs];
            u32 d = regs[rt];
            if (d == 0) {
                hi = n;
                lo = 0xFFFFFFFF;
            }
            else {
                lo = n / d;
                hi = n % d;
            }
            break;
        }

        case 0x20: // ADD (identique a ADDU tant que les exceptions
            // d'overflow COP0 ne sont pas implementees)
            regs[rd] = regs[rs] + regs[rt];
            break;
        case 0x21: // ADDU
            regs[rd] = regs[rs] + regs[rt];
            break;
        case 0x22: // SUB
            regs[rd] = regs[rs] - regs[rt];
            break;
        case 0x23: // SUBU
            regs[rd] = regs[rs] - regs[rt];
            break;

        case 0x24: // AND (bit a bit : &, pas &&)
            regs[rd] = regs[rs] & regs[rt];
            break;
        case 0x25: // OR
            regs[rd] = regs[rs] | regs[rt];
            break;
        case 0x26: // XOR
            regs[rd] = regs[rs] ^ regs[rt];
            break;
        case 0x27: // NOR
            regs[rd] = ~(regs[rs] | regs[rt]);
            break;

        case 0x2A: // SLT (comparaison signee)
            regs[rd] = ((s32)regs[rs] < (s32)regs[rt]) ? 1u : 0u;
            break;
        case 0x2B: // SLTU (comparaison non signee)
            regs[rd] = (regs[rs] < regs[rt]) ? 1u : 0u;
            break;

        default:
            std::cerr << "Unknown funct: 0x" << std::hex << funct << "\n";
        }
        break; // <-- indispensable, sinon on tombe dans le case 0x01 !
    }

    case 0x01: { // Opcode partage : BLTZ/BGEZ/BLTZAL/BGEZAL selon rt
        bool ge = ((s32)regs[rs] >= 0);
        bool link = (rt == 0x10 || rt == 0x11); // BLTZAL / BGEZAL
        bool take = (rt & 1) ? ge : !ge;        // rt impair = BGEZ(AL)
        if (link)
            regs[RA] = next_pc; // ra = $+8, meme si non pris
        if (take)
            next_pc = branch_dest;
        break;
    }

    case 0x02: // J : champ 26 bits, meme page 256MB que le delay slot
        next_pc = (pc & 0xF0000000) | (imm26 << 2);
        break;
    case 0x03: // JAL : idem + ra = $+8
        regs[RA] = next_pc;
        next_pc = (pc & 0xF0000000) | (imm26 << 2);
        break;

    case 0x04: // BEQ
        if (regs[rs] == regs[rt])
            next_pc = branch_dest;
        break;
    case 0x05: // BNE
        if (regs[rs] != regs[rt])
            next_pc = branch_dest;
        break;
    case 0x06: // BLEZ (comparaison signee !)
        if ((s32)regs[rs] <= 0)
            next_pc = branch_dest;
        break;
    case 0x07: // BGTZ
        if ((s32)regs[rs] > 0)
            next_pc = branch_dest;
        break;

    case 0x08: // ADDI (immediat etendu avec son signe)
        regs[rt] = regs[rs] + (s16)imm;
        break;
    case 0x09: // ADDIU : ATTENTION, le "U" ne veut PAS dire immediat
        // non signe ! Extension de signe comme ADDI, la seule
        // difference est l'absence d'exception d'overflow.
        regs[rt] = regs[rs] + (s16)imm;
        break;
    case 0x10: {// COP0
        switch (rs) {   // rs sert de sous-opcode pour les coprocesseurs
        case 0x00: // MFC0 : COP0[rd] -> regs[rt]
            regs[rt] = cop0_regs[rd];
            break;
        case 0x04: // MTC0 : regs[rt] -> COP0[rd]
            cop0_regs[rd] = regs[rt];
            break;
        default:
            std::cerr << "Unknown COP0 instruction: 0x" << std::hex << rs << "\n";
        }
        break;
    case 0x0A: // SLTI (comparaison signee)
        regs[rt] = ((s32)regs[rs] < (s32)(s16)imm) ? 1u : 0u;
        break;
    case 0x0B: // SLTIU (comparaison non signee, mais l'immediat
        // est quand meme etendu avec son signe d'abord)
        regs[rt] = (regs[rs] < (u32)(s32)(s16)imm) ? 1u : 0u;
        break;
    case 0x0C: // ANDI (immediat etendu avec des ZEROS, lui)
        regs[rt] = regs[rs] & imm;
        break;
    case 0x0D: // ORI (zero-extended)
        regs[rt] = regs[rs] | imm;
        break;
    case 0x0E: // XORI (zero-extended)
        regs[rt] = regs[rs] ^ imm;
        break;
    case 0x0F: // LUI : place l'immediat dans les 16 bits hauts
        regs[rt] = imm << 16;
        break;

    case 0x20: // LB (extension de signe)
        regs[rt] = (s8)memoire.load8(regs[rs] + (s16)imm);
        break;
    case 0x21: // LH (extension de signe)
        regs[rt] = (s16)memoire.load16(regs[rs] + (s16)imm);
        break;
    case 0x23: // LW
        regs[rt] = memoire.load32(regs[rs] + (s16)imm);
        break;
    case 0x24: // LBU (rempli avec des zeros)
        regs[rt] = memoire.load8(regs[rs] + (s16)imm);
        break;
    case 0x25: // LHU
        regs[rt] = memoire.load16(regs[rs] + (s16)imm);
        break;

    case 0x28: // SB (ecrit l'octet bas du registre)
        if (!cacheIsolated())
            memoire.store8(regs[rs] + (s16)imm, (u8)regs[rt]);
        break;
    case 0x29: // SH (ecrit la moitie basse)
        if (!cacheIsolated())
            memoire.store16(regs[rs] + (s16)imm, (u16)regs[rt]);
        break;
    case 0x2B: // SW
        if (!cacheIsolated())
            memoire.store32(regs[rs] + (s16)imm, regs[rt]);
        break;

    default:
        std::cerr << "Unknown opcode: 0x" << std::hex << opcode << "\n";
    }

             regs[0] = 0; // le registre zero vaut toujours 0
    }
}