#include "emu.h"
#include <iostream>
#include <array>
#include <cstdint>

constexpr size_t RAM_SIZE = 2 * 1024 * 1024;//2MB
constexpr uint32_t BIOS_START = 0x1FC00000;
constexpr size_t BIOS_SIZE = 512 * 1024;

class Memory {
    std::array<uint8_t, RAM_SIZE> ram{};
    std::array<uint8_t, BIOS_SIZE> bios{};

public:
    uint32_t load32(uint32_t addr) {
        if (addr < RAM_SIZE) {
            return *(uint32_t*)&ram[addr];
        }
        else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
            return *(uint32_t*)&bios[addr - BIOS_START];
        }
        else {
            std::cerr << "Read from unknown address: 0x" << std::hex << addr << "\n";
            return 0;
        }
    }

    void store32(uint32_t addr, uint32_t value) {
        if (addr < RAM_SIZE) {
            *(uint32_t*)&ram[addr] = value;
        }
        else {
            std::cerr << "Write to unknown address: 0x" << std::hex << addr << "\n";
        }
    }

    void loadBIOS(const std::string& filename) {
        FILE* f = fopen(filename.c_str(), "rb");
        if (!f) {
            std::cerr << "Failed to open BIOS file\n";
            exit(1);
        }
        fread(bios.data(), 1, BIOS_SIZE, f);
        fclose(f);
    }

    void loadTestProgram() {
        uint32_t program[] = {
            0x20010005, // ADDI r1, r0, 5
            0x2002000A, // ADDI r2, r0, 10
            0x00221820, // ADD r3, r1, r2
            0xAC031000, // SW r3, 0x1000(r0)
            0x8C041000, // LW r4, 0x1000(r0)
            0x1000FFFF  // BEQ r0, r0, -1 (boucle infinie)
        };

        for (size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i) {
            *(uint32_t*)&ram[i * 4] = program[i];
        }
    }
};


class CPU {
    std::array<uint32_t, 32> regs{};
    uint32_t pc = 0x00000000;
    Memory& memory;

public:
    CPU(Memory& mem) : memory(mem) {}

    void step() {
        uint32_t instr = memory.load32(pc);
        pc += 4;

        uint32_t opcode = instr >> 26;
        uint32_t rs = (instr >> 21) & 0x1F;
        uint32_t rt = (instr >> 16) & 0x1F;
        uint32_t rd = (instr >> 11) & 0x1F;
        uint32_t imm = instr & 0xFFFF;

        switch (opcode) {
        case 0x00: { // R-type (e.g. ADD)
            uint32_t funct = instr & 0x3F;
            switch (funct) {
            case 0x00:// SLL

                break;
            case 0x20:
                regs[rd] = regs[rs] + regs[rt];

                std::cout << "ADD r" << rd << " = r" << rs << " + r" << rt << "\n"; //logs console
                break;
            case 0x21:// ADDU

                break;
            case 0x22:// SUB

                break;
            case 0x23:// SUBU

                break;
            case 0x24:// AND

                break;
            case 0x25:// OR

                break;
            case 0x26:// XOR

                break;
            case 0x2A:// SLT

                break;
            case 0x2B:// SLTU

                break;
            }



            /*if (funct == 0x20) { // ADD
                regs[rd] = regs[rs] + regs[rt];

                std::cout << "ADD r" << rd << " = r" << rs << " + r" << rt << "\n"; //logs console
            }*/
            else if (funct == 0x21) {// ADDU

            }
            else if (funct == 0x22) {// SUB

            }
            else if (funct == 0x23) {// SUBU

            }
            else if (funct == 0x24) {// AND

            }
            else if (funct == 0x25) {// OR

            }
            else if (funct == 0x26) {// XOR

            }
            else if (funct == 0x24) {// AND

            }
            else if (funct == 0x24) {// AND

            }
            else if (funct == 0x24) {// AND

            }
            else if (funct == 0x2A) {// SLT

            }
            else if (funct == 0x2B) {// SLTU

            }
            break;
        }
        case 0x08: // ADDI
            regs[rt] = regs[rs] + (int16_t)imm;

            std::cout << "ADDI r" << rt << " = r" << rs << " + " << (int16_t)imm << "\n"; //logs console
            break;
        case 0x23: // LW
            regs[rt] = memory.load32(regs[rs] + (int16_t)imm);

            std::cout << "LW r" << rt << " <- [" << regs[rs] + (int16_t)imm << "]\n"; //logs console
            break;
        case 0x2B: // SW
            memory.store32(regs[rs] + (int16_t)imm, regs[rt]);

            std::cout << "SW [" << regs[rs] + (int16_t)imm << "] <- r" << rt << "\n"; //logs console
            break;
        case 0x04: // BEQ
            if (regs[rs] == regs[rt]) {
                pc += ((int16_t)imm << 2);

                std::cout << "BEQ taken\n";//logs console
            }
            break;
        default:
            std::cerr << "Unknown opcode: " << std::hex << opcode << "\n";
        }

        regs[0] = 0; // registre zéro toujours égal à 0
    }
};

class Emulator {
    Memory memory;
    CPU cpu;



public:
    Emulator() : cpu(memory) {}

    void loadTestProgram() {
        memory.loadTestProgram();
    }

    void loadBIOS(const std::string& path) {
        memory.loadBIOS(path);
    }

    void run(int steps = 10) {
        for (int i = 0; i < steps; ++i) {
            cpu.step();
        }
    }
};

int main() {
    Emulator emu;
    emu.loadTestProgram(); // charge notre mini programme
    emu.run(20); // exécute 20 instructions
    return 0;
}