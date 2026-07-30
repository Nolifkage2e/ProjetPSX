#include "Desassembleur.h"
#include <cstdio>

static const char* NOMS_REGS[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0",   "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0",   "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8",   "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

const char* nomRegistre(u32 index) {
    return (index < 32) ? NOMS_REGS[index] : "??";
}

static const char* NOMS_COP0[32] = {
    "r0","r1","r2","BPC","r4","BDA","JUMPDEST","DCIC",
    "BadVaddr","BDAM","r10","BPCM","SR","CAUSE","EPC","PRID",
    "r16","r17","r18","r19","r20","r21","r22","r23",
    "r24","r25","r26","r27","r28","r29","r30","r31"
};

std::string desassembler(u32 instr, u32 addr) {
    char buf[96];

    u32 op = instr >> 26;
    u32 rs = (instr >> 21) & 0x1F;
    u32 rt = (instr >> 16) & 0x1F;
    u32 rd = (instr >> 11) & 0x1F;
    u32 shamt = (instr >> 6) & 0x1F;
    u32 funct = instr & 0x3F;
    s16 imm = (s16)(instr & 0xFFFF);
    u32 imm26 = instr & 0x03FFFFFF;

    // Cible d'un branchement : addr + 4 + offset*4
    u32 cible_branche = addr + 4 + ((s32)imm << 2);
    u32 cible_saut = ((addr + 4) & 0xF0000000) | (imm26 << 2);

    const char* R = nullptr;   // nom des registres, raccourcis
#define RS nomRegistre(rs)
#define RT nomRegistre(rt)
#define RD nomRegistre(rd)

    switch (op) {
    case 0x00:   // R-type
        switch (funct) {
        case 0x00:
            if (instr == 0) return "nop";
            snprintf(buf, sizeof(buf), "sll    %s, %s, %u", RD, RT, shamt); break;
        case 0x02: snprintf(buf, sizeof(buf), "srl    %s, %s, %u", RD, RT, shamt); break;
        case 0x03: snprintf(buf, sizeof(buf), "sra    %s, %s, %u", RD, RT, shamt); break;
        case 0x04: snprintf(buf, sizeof(buf), "sllv   %s, %s, %s", RD, RT, RS); break;
        case 0x06: snprintf(buf, sizeof(buf), "srlv   %s, %s, %s", RD, RT, RS); break;
        case 0x07: snprintf(buf, sizeof(buf), "srav   %s, %s, %s", RD, RT, RS); break;
        case 0x08: snprintf(buf, sizeof(buf), "jr     %s", RS); break;
        case 0x09: snprintf(buf, sizeof(buf), "jalr   %s, %s", RD, RS); break;
        case 0x0C: snprintf(buf, sizeof(buf), "syscall"); break;
        case 0x0D: snprintf(buf, sizeof(buf), "break"); break;
        case 0x10: snprintf(buf, sizeof(buf), "mfhi   %s", RD); break;
        case 0x11: snprintf(buf, sizeof(buf), "mthi   %s", RS); break;
        case 0x12: snprintf(buf, sizeof(buf), "mflo   %s", RD); break;
        case 0x13: snprintf(buf, sizeof(buf), "mtlo   %s", RS); break;
        case 0x18: snprintf(buf, sizeof(buf), "mult   %s, %s", RS, RT); break;
        case 0x19: snprintf(buf, sizeof(buf), "multu  %s, %s", RS, RT); break;
        case 0x1A: snprintf(buf, sizeof(buf), "div    %s, %s", RS, RT); break;
        case 0x1B: snprintf(buf, sizeof(buf), "divu   %s, %s", RS, RT); break;
        case 0x20: snprintf(buf, sizeof(buf), "add    %s, %s, %s", RD, RS, RT); break;
        case 0x21:
            if (rt == 0) snprintf(buf, sizeof(buf), "move   %s, %s", RD, RS);
            else snprintf(buf, sizeof(buf), "addu   %s, %s, %s", RD, RS, RT);
            break;
        case 0x22: snprintf(buf, sizeof(buf), "sub    %s, %s, %s", RD, RS, RT); break;
        case 0x23: snprintf(buf, sizeof(buf), "subu   %s, %s, %s", RD, RS, RT); break;
        case 0x24: snprintf(buf, sizeof(buf), "and    %s, %s, %s", RD, RS, RT); break;
        case 0x25: snprintf(buf, sizeof(buf), "or     %s, %s, %s", RD, RS, RT); break;
        case 0x26: snprintf(buf, sizeof(buf), "xor    %s, %s, %s", RD, RS, RT); break;
        case 0x27: snprintf(buf, sizeof(buf), "nor    %s, %s, %s", RD, RS, RT); break;
        case 0x2A: snprintf(buf, sizeof(buf), "slt    %s, %s, %s", RD, RS, RT); break;
        case 0x2B: snprintf(buf, sizeof(buf), "sltu   %s, %s, %s", RD, RS, RT); break;
        default:   snprintf(buf, sizeof(buf), "??? funct=0x%02X", funct); break;
        }
        break;

    case 0x01: {
        const char* nom = "?";
        if (rt == 0x00) nom = "bltz";
        else if (rt == 0x01) nom = "bgez";
        else if (rt == 0x10) nom = "bltzal";
        else if (rt == 0x11) nom = "bgezal";
        snprintf(buf, sizeof(buf), "%-6s %s, 0x%08X", nom, RS, cible_branche);
        break;
    }

    case 0x02: snprintf(buf, sizeof(buf), "j      0x%08X", cible_saut); break;
    case 0x03: snprintf(buf, sizeof(buf), "jal    0x%08X", cible_saut); break;
    case 0x04:
        if (rt == 0) snprintf(buf, sizeof(buf), "beqz   %s, 0x%08X", RS, cible_branche);
        else snprintf(buf, sizeof(buf), "beq    %s, %s, 0x%08X", RS, RT, cible_branche);
        break;
    case 0x05:
        if (rt == 0) snprintf(buf, sizeof(buf), "bnez   %s, 0x%08X", RS, cible_branche);
        else snprintf(buf, sizeof(buf), "bne    %s, %s, 0x%08X", RS, RT, cible_branche);
        break;
    case 0x06: snprintf(buf, sizeof(buf), "blez   %s, 0x%08X", RS, cible_branche); break;
    case 0x07: snprintf(buf, sizeof(buf), "bgtz   %s, 0x%08X", RS, cible_branche); break;

    case 0x08: snprintf(buf, sizeof(buf), "addi   %s, %s, %d", RT, RS, imm); break;
    case 0x09:
        if (rs == 0) snprintf(buf, sizeof(buf), "li     %s, %d", RT, imm);
        else snprintf(buf, sizeof(buf), "addiu  %s, %s, %d", RT, RS, imm);
        break;
    case 0x0A: snprintf(buf, sizeof(buf), "slti   %s, %s, %d", RT, RS, imm); break;
    case 0x0B: snprintf(buf, sizeof(buf), "sltiu  %s, %s, %d", RT, RS, imm); break;
    case 0x0C: snprintf(buf, sizeof(buf), "andi   %s, %s, 0x%04X", RT, RS, (u16)imm); break;
    case 0x0D: snprintf(buf, sizeof(buf), "ori    %s, %s, 0x%04X", RT, RS, (u16)imm); break;
    case 0x0E: snprintf(buf, sizeof(buf), "xori   %s, %s, 0x%04X", RT, RS, (u16)imm); break;
    case 0x0F: snprintf(buf, sizeof(buf), "lui    %s, 0x%04X", RT, (u16)imm); break;

    case 0x10:   // COP0
        if (rs & 0x10) {
            if ((instr & 0x3F) == 0x10) snprintf(buf, sizeof(buf), "rfe");
            else snprintf(buf, sizeof(buf), "cop0   0x%08X", instr);
        }
        else if (rs == 0x00) snprintf(buf, sizeof(buf), "mfc0   %s, %s", RT, NOMS_COP0[rd]);
        else if (rs == 0x04) snprintf(buf, sizeof(buf), "mtc0   %s, %s", RT, NOMS_COP0[rd]);
        else snprintf(buf, sizeof(buf), "cop0   ???");
        break;

    case 0x12: snprintf(buf, sizeof(buf), "cop2   0x%08X  (GTE)", instr); break;

    case 0x20: snprintf(buf, sizeof(buf), "lb     %s, %d(%s)", RT, imm, RS); break;
    case 0x21: snprintf(buf, sizeof(buf), "lh     %s, %d(%s)", RT, imm, RS); break;
    case 0x22: snprintf(buf, sizeof(buf), "lwl    %s, %d(%s)", RT, imm, RS); break;
    case 0x23: snprintf(buf, sizeof(buf), "lw     %s, %d(%s)", RT, imm, RS); break;
    case 0x24: snprintf(buf, sizeof(buf), "lbu    %s, %d(%s)", RT, imm, RS); break;
    case 0x25: snprintf(buf, sizeof(buf), "lhu    %s, %d(%s)", RT, imm, RS); break;
    case 0x26: snprintf(buf, sizeof(buf), "lwr    %s, %d(%s)", RT, imm, RS); break;
    case 0x28: snprintf(buf, sizeof(buf), "sb     %s, %d(%s)", RT, imm, RS); break;
    case 0x29: snprintf(buf, sizeof(buf), "sh     %s, %d(%s)", RT, imm, RS); break;
    case 0x2A: snprintf(buf, sizeof(buf), "swl    %s, %d(%s)", RT, imm, RS); break;
    case 0x2B: snprintf(buf, sizeof(buf), "sw     %s, %d(%s)", RT, imm, RS); break;
    case 0x2E: snprintf(buf, sizeof(buf), "swr    %s, %d(%s)", RT, imm, RS); break;

    default: snprintf(buf, sizeof(buf), "??? op=0x%02X", op); break;
    }

#undef RS
#undef RT
#undef RD
    return std::string(buf);
}