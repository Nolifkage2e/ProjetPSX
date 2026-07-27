#include "Memoire.h"
#include <iostream>
#include <cstdio>

u32 Memoire::load32(u32 addr) {
    if (addr < RAM_SIZE) {
        return *(u32*)&ram[addr];
    }
    else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
        return *(u32*)&bios[addr - BIOS_START];
    }
    else {
        std::cerr << "Read32 from unknown address: 0x" << std::hex << addr << "\n";
        return 0;
    }
}

u16 Memoire::load16(u32 addr) {
    if (addr < RAM_SIZE) {
        return *(u16*)&ram[addr];
    }
    else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
        return *(u16*)&bios[addr - BIOS_START];
    }
    else {
        std::cerr << "Read16 from unknown address: 0x" << std::hex << addr << "\n";
        return 0;
    }
}

u8 Memoire::load8(u32 addr) {
    if (addr < RAM_SIZE) {
        return ram[addr];
    }
    else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
        return bios[addr - BIOS_START];
    }
    else {
        std::cerr << "Read8 from unknown address: 0x" << std::hex << addr << "\n";
        return 0;
    }
}

void Memoire::store32(u32 addr, u32 value) {
    if (addr < RAM_SIZE) {
        *(u32*)&ram[addr] = value;
    }
    else {
        std::cerr << "Write32 to unknown address: 0x" << std::hex << addr << "\n";
    }
}

void Memoire::store16(u32 addr, u16 value) {
    if (addr < RAM_SIZE) {
        *(u16*)&ram[addr] = value;
    }
    else {
        std::cerr << "Write16 to unknown address: 0x" << std::hex << addr << "\n";
    }
}

void Memoire::store8(u32 addr, u8 value) {
    if (addr < RAM_SIZE) {
        ram[addr] = value;
    }
    else {
        std::cerr << "Write8 to unknown address: 0x" << std::hex << addr << "\n";
    }
}

void Memoire::loadBIOS(const std::string& filename) {
    FILE* f = nullptr;
    fopen_s(&f, filename.c_str(), "rb");
    if (!f) {
        std::cerr << "Failed to open BIOS file\n";
        exit(1);
    }
    fread(bios.data(), 1, BIOS_SIZE, f);
    fclose(f);
}

void Memoire::loadTestProgram() {
    u32 program[] = {
        0x20010005, // ADDI r1, r0, 5
        0x2002000A, // ADDI r2, r0, 10
        0x00221820, // ADD  r3, r1, r2
        0xAC031000, // SW   r3, 0x1000(r0)
        0x8C041000, // LW   r4, 0x1000(r0)
        0x1000FFFF  // BEQ  r0, r0, -1 (boucle infinie)
    };
    for (size_t i = 0; i < sizeof(program) / sizeof(program[0]); ++i) {
        *(u32*)&ram[i * 4] = program[i];
    }
}