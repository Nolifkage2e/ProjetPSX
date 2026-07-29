#include "Cpu.h"
#include <iostream>

void CPU::step() {
    current_pc = pc;

    checkInterrupts();
    if (pc != current_pc) {
        // une exception vient d'être prise : on sort, le vecteur s'exécutera au prochain step
        return;
    }

    u32 instr = memoire.load32(pc);

    in_delay_slot = branch_taken;
    branch_taken = false;

    pc = next_pc;
    next_pc += 4;      // next_pc = $+8


    // --- LOAD DELAY SLOT ---
    // Le load du step precedent devient disponible MAINTENANT.
    // On garde sa valeur de cote, on execute l'instruction courante
    // (qui peut ecrire dans le meme registre = load annule), puis on
    // applique le load en attente a la fin.
    u32 pending_reg = load_reg;
    u32 pending_value = load_value;
    load_reg = 0;
    load_value = 0;

    // Ecrit dans un registre "tout de suite", en annulant un load
    // en attente qui viserait le meme registre (l'ecriture directe gagne).
    auto setReg = [&](u32 index, u32 value) {
        if (pending_reg == index) pending_reg = 0;
        regs[index] = value;
        };

    // Decodage des champs
    u32 opcode = instr >> 26;
    u32 rs = (instr >> 21) & 0x1F;
    u32 rt = (instr >> 16) & 0x1F;
    u32 rd = (instr >> 11) & 0x1F;
    u32 shamt = (instr >> 6) & 0x1F;
    u32 imm = instr & 0xFFFF;
    u32 imm26 = instr & 0x03FFFFFF;

    u32 branch_dest = pc + ((s16)imm << 2);

    switch (opcode) {

    case 0x00: { // R-type
        u32 funct = instr & 0x3F;
        switch (funct) {
        case 0x00: setReg(rd, regs[rt] << shamt); break;               // SLL
        case 0x02: setReg(rd, regs[rt] >> shamt); break;               // SRL
        case 0x03: setReg(rd, (s32)regs[rt] >> shamt); break;          // SRA
        case 0x04: setReg(rd, regs[rt] << (regs[rs] & 0x1F)); break;   // SLLV
        case 0x06: setReg(rd, regs[rt] >> (regs[rs] & 0x1F)); break;   // SRLV
        case 0x07: setReg(rd, (s32)regs[rt] >> (regs[rs] & 0x1F)); break; // SRAV

        case 0x08: next_pc = regs[rs]; branch_taken = true; break;                          // JR
        case 0x09: setReg(rd, next_pc); next_pc = regs[rs]; branch_taken = true; break;     // JALR
        case 0x0C: // SYSCALL
            exception(EXC_SYSCALL);
            break;
        case 0x0D: // BREAK
            exception(EXC_BREAK);
            break;
        case 0x10: setReg(rd, hi); break;                              // MFHI
        case 0x11: hi = regs[rs]; break;                               // MTHI
        case 0x12: setReg(rd, lo); break;                              // MFLO
        case 0x13: lo = regs[rs]; break;                               // MTLO

        case 0x18: { // MULT
            s64 r = (s64)(s32)regs[rs] * (s64)(s32)regs[rt];
            hi = (u32)((u64)r >> 32); lo = (u32)r; break;
        }
        case 0x19: { // MULTU
            u64 r = (u64)regs[rs] * (u64)regs[rt];
            hi = (u32)(r >> 32); lo = (u32)r; break;
        }
        case 0x1A: { // DIV
            s32 n = (s32)regs[rs], d = (s32)regs[rt];
            if (d == 0) { hi = (u32)n; lo = (n >= 0) ? 0xFFFFFFFF : 1; }
            else if ((u32)n == 0x80000000 && d == -1) { hi = 0; lo = 0x80000000; }
            else { lo = (u32)(n / d); hi = (u32)(n % d); }
            break;
        }
        case 0x1B: { // DIVU
            u32 n = regs[rs], d = regs[rt];
            if (d == 0) { hi = n; lo = 0xFFFFFFFF; }
            else { lo = n / d; hi = n % d; }
            break;
        }

        case 0x20: setReg(rd, regs[rs] + regs[rt]); break;             // ADD
        case 0x21: setReg(rd, regs[rs] + regs[rt]); break;             // ADDU
        case 0x22: setReg(rd, regs[rs] - regs[rt]); break;             // SUB
        case 0x23: setReg(rd, regs[rs] - regs[rt]); break;             // SUBU
        case 0x24: setReg(rd, regs[rs] & regs[rt]); break;             // AND
        case 0x25: setReg(rd, regs[rs] | regs[rt]); break;             // OR
        case 0x26: setReg(rd, regs[rs] ^ regs[rt]); break;             // XOR
        case 0x27: setReg(rd, ~(regs[rs] | regs[rt])); break;          // NOR
        case 0x2A: setReg(rd, ((s32)regs[rs] < (s32)regs[rt]) ? 1u : 0u); break; // SLT
        case 0x2B: setReg(rd, (regs[rs] < regs[rt]) ? 1u : 0u); break; // SLTU

        default:
            std::cerr << "Unknown funct: 0x" << std::hex << funct << "\n";
        }
        break;
    }

    case 0x01: {
        bool ge = ((s32)regs[rs] >= 0);
        bool link = (rt == 0x10 || rt == 0x11);
        bool take = (rt & 1) ? ge : !ge;
        if (link) setReg(RA, next_pc);
        if (take) { next_pc = branch_dest; branch_taken = true; }
        break;
    }

    case 0x02: // J
    {
        next_pc = (pc & 0xF0000000) | (imm26 << 2);
        branch_taken = true;
        break;
    }
    case 0x03: { // JAL
        setReg(RA, next_pc);
        next_pc = (pc & 0xF0000000) | (imm26 << 2);
        branch_taken = true;
        break;
    }

    case 0x04:
        if (regs[rs] == regs[rt])
        { 
            next_pc = branch_dest;
            branch_taken = true; 
        } 
        break; // BEQ
    case 0x05:
        if (regs[rs] != regs[rt]) {
            next_pc = branch_dest;
            branch_taken = true;
        }
        break; // BNE
    case 0x06:
        if ((s32)regs[rs] <= 0)
        { next_pc = branch_dest;
        branch_taken = true; }
        break;   // BLEZ
    case 0x07:
        if ((s32)regs[rs] > 0)
        { next_pc = branch_dest;
        branch_taken = true; }
        break;  // BGTZ
    case 0x08: setReg(rt, regs[rs] + (s16)imm); break;                 // ADDI
    case 0x09: setReg(rt, regs[rs] + (s16)imm); break;                 // ADDIU
    case 0x10: { // COP0
        if (rs & 0x10) {
            // Bit 4 de rs leve => operation coprocesseur (RFE et cie)
            u32 cop_funct = instr & 0x3F;
            if (cop_funct == 0x10) { // RFE : Return From Exception
                // Depile la pile de mode de SR : decale les bits 0-5
                // de 2 vers la DROITE (previous -> current).
                u32 sr = cop0_regs[COP0_SR];
                u32 mode = sr & 0x3F;
                sr &= ~0xF;                 // efface bits 0-3
                sr |= (mode >> 2) & 0xF;    // recopie prev/old vers current/prev
                cop0_regs[COP0_SR] = sr;
            }
            else {
                std::cerr << "COP0 op inconnue, funct: 0x" << std::hex << cop_funct << "\n";
            }
        }
        else {
            // Bit 4 a 0 => transfert de registre (MFC0 / MTC0)
            switch (rs) {
            case 0x00: setReg(rt, cop0_regs[rd]); break;  // MFC0 : COP0[rd] -> rt
            case 0x04: cop0_regs[rd] = regs[rt];  break;  // MTC0 : rt -> COP0[rd]
            default:
                std::cerr << "Unknown COP0 instruction: 0x" << std::hex << rs << "\n";
            }
        }
        break;
    }  // <-- ACCOLADE qui manquait : ferme le case COP0


    case 0x0A: setReg(rt, ((s32)regs[rs] < (s32)(s16)imm) ? 1u : 0u); break; // SLTI
    case 0x0B: setReg(rt, (regs[rs] < (u32)(s32)(s16)imm) ? 1u : 0u); break; // SLTIU
    case 0x0C: setReg(rt, regs[rs] & imm); break;                      // ANDI
    case 0x0D: setReg(rt, regs[rs] | imm); break;                      // ORI
    case 0x0E: setReg(rt, regs[rs] ^ imm); break;                      // XORI
    case 0x0F: setReg(rt, imm << 16); break;                           // LUI

        // --- LOADS : programment le slot en attente (delai d'un cycle) ---
    case 0x20: load_reg = rt; load_value = (s8)memoire.load8(regs[rs] + (s16)imm); break; // LB
    case 0x21: load_reg = rt; load_value = (s16)memoire.load16(regs[rs] + (s16)imm); break; // LH
    case 0x22: { // LWL
        u32 vaddr = regs[rs] + (s16)imm;
        u32 aligned = vaddr & ~3;
        u32 offset = vaddr & 3;
        u32 mem = memoire.load32(aligned);

        // Bypass : si une instruction précédente chargeait déjà ce registre, on fusionne avec la nouvelle valeur
        u32 current_rt = (rt == pending_reg) ? pending_value : regs[rt];

        switch (offset) {
        case 0: load_value = (current_rt & 0x00FFFFFF) | (mem << 24); break;
        case 1: load_value = (current_rt & 0x0000FFFF) | (mem << 16); break;
        case 2: load_value = (current_rt & 0x000000FF) | (mem << 8);  break;
        case 3: load_value = mem; break;
        }
        load_reg = rt;
        break;
    }
    case 0x23: load_reg = rt; load_value = memoire.load32(regs[rs] + (s16)imm); break;      // LW
    case 0x24: load_reg = rt; load_value = memoire.load8(regs[rs] + (s16)imm); break;      // LBU
    case 0x25: load_reg = rt; load_value = memoire.load16(regs[rs] + (s16)imm); break;      // LHU
    case 0x26: { // LWR
        u32 vaddr = regs[rs] + (s16)imm;
        u32 aligned = vaddr & ~3;
        u32 offset = vaddr & 3;
        u32 mem = memoire.load32(aligned);

        u32 current_rt = (rt == pending_reg) ? pending_value : regs[rt];

        switch (offset) {
        case 0: load_value = mem; break;
        case 1: load_value = (current_rt & 0xFF000000) | (mem >> 8);  break;
        case 2: load_value = (current_rt & 0xFFFF0000) | (mem >> 16); break;
        case 3: load_value = (current_rt & 0xFFFFFF00) | (mem >> 24); break;
        }
        load_reg = rt;
        break;
    }

        // --- STORES : effet immediat ---
    case 0x28: if (!cacheIsolated()) memoire.store8(regs[rs] + (s16)imm, (u8)regs[rt]); break;  // SB
    case 0x29: if (!cacheIsolated()) memoire.store16(regs[rs] + (s16)imm, (u16)regs[rt]); break; // SH
    case 0x2A: { // SWL
        if (!cacheIsolated()) {
            u32 vaddr = regs[rs] + (s16)imm;
            u32 aligned = vaddr & ~3;
            u32 offset = vaddr & 3;
            u32 mem = memoire.load32(aligned);
            u32 val = regs[rt];
            switch (offset) {
            case 0: memoire.store32(aligned, (mem & 0xFFFFFF00) | (val >> 24)); break;
            case 1: memoire.store32(aligned, (mem & 0xFFFF0000) | (val >> 16)); break;
            case 2: memoire.store32(aligned, (mem & 0xFF000000) | (val >> 8));  break;
            case 3: memoire.store32(aligned, val); break;
            }
        }
        break;
    }
    case 0x2B: if (!cacheIsolated()) memoire.store32(regs[rs] + (s16)imm, regs[rt]); break;     // SW
    case 0x2E: { // SWR
        if (!cacheIsolated()) {
            u32 vaddr = regs[rs] + (s16)imm;
            u32 aligned = vaddr & ~3;
            u32 offset = vaddr & 3;
            u32 mem = memoire.load32(aligned);
            u32 val = regs[rt];
            switch (offset) {
            case 0: memoire.store32(aligned, val); break;
            case 1: memoire.store32(aligned, (mem & 0x000000FF) | (val << 8));  break;
            case 2: memoire.store32(aligned, (mem & 0x0000FFFF) | (val << 16)); break;
            case 3: memoire.store32(aligned, (mem & 0x00FFFFFF) | (val << 24)); break;
            }
        }
        break;
    }
    default:
        std::cerr << "Unknown opcode: 0x" << std::hex << opcode << "\n";
    }

    // --- Appliquer le load en attente (celui du step precedent) ---
    if (pending_reg != 0) {
        regs[pending_reg] = pending_value;
    }

    regs[0] = 0; // le registre zero vaut toujours 0
}

void CPU::exception(u32 cause) {
    std::cout << "EXC cause=" << std::hex << cause
        << " SR_avant=0x" << cop0_regs[COP0_SR] << "\n";

    u32 sr = cop0_regs[COP0_SR];
    u32 handler = (sr & (1 << 22)) ? 0xBFC00180 : 0x80000080;

    // Pile de mode
    u32 mode = sr & 0x3F;
    sr &= ~0x3F;
    sr |= (mode << 2) & 0x3F;
    cop0_regs[COP0_SR] = sr;

    // CAUSE : code de cause
    u32 causeReg = cop0_regs[COP0_CAUSE];
    causeReg &= ~0x7C;
    causeReg |= (cause << 2) & 0x7C;

    // --- GESTION DU DELAY SLOT ---
    u32 epc = current_pc;
    if (in_delay_slot) {
        epc -= 4;                    // EPC pointe sur le BRANCHEMENT
        causeReg |= (1u << 31);      // bit BD : "l'exception était dans un delay slot"
    }
    else {
        causeReg &= ~(1u << 31);
    }
    cop0_regs[COP0_CAUSE] = causeReg;
    cop0_regs[COP0_EPC] = epc;

    pc = handler;
    next_pc = handler + 4;
}

// ------------------------------------------------------------
//  checkInterrupts() : a appeler a chaque step.
//  Prend une interruption materielle si les conditions sont reunies.
// ------------------------------------------------------------
void CPU::checkInterrupts() {
    // Y a-t-il une IRQ en attente cote bus (I_STAT & I_MASK) ?
    bool irqPending = memoire.getIrq().pending();

    // Reflete l'etat dans le bit 10 de CAUSE (IP2 = interruption materielle).
    // Le hardware recopie la ligne d'interruption dans CAUSE.bit10.
    if (irqPending) {
        cop0_regs[COP0_CAUSE] |= (1 << 10);
    }
    else {
        cop0_regs[COP0_CAUSE] &= ~(1 << 10);
    }

    u32 sr = cop0_regs[COP0_SR];

    // Conditions pour prendre l'interruption :
    //   - IEc (bit 0 de SR) : interruptions globalement activees
    //   - le bit de masque correspondant dans SR (bit 10 = IM2) est leve
    //   - le bit correspondant dans CAUSE (bit 10 = IP2) est leve
    bool globalEnable = sr & 0x1;
    bool maskAndPending = (sr & cop0_regs[COP0_CAUSE] & 0xFF00) != 0;

    if (globalEnable && maskAndPending) {
        exception(EXC_INT);  // cause 0 = interruption
    }
}