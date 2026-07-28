#include "Memoire.h"
#include "Emulateur.h"
#include <iostream>
#include <cstdio>
#include <algorithm>
#include "Dma.h" 


static const u32 REGION_MASK[8] = {
    // KUSEG : 4 tranches de 512 Mo, adresse inchangée
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    // KSEG0 : on retire le bit 31
    0x7FFFFFFF,
    // KSEG1 : on retire les bits 31-29
    0x1FFFFFFF,
    // KSEG2 : à part (cache control), on n'y touche pas pour l'instant
    0xFFFFFFFF, 0xFFFFFFFF,
};

static u32 mask_region(u32 addr) {
    return addr & REGION_MASK[addr >> 29];
}

u32 Memoire::load32(u32 addr) {
    addr = mask_region(addr);
    if (addr < RAM_SIZE) {
        return *(u32*)&ram[addr];
    }
    else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
        return *(u32*)&bios[addr - BIOS_START];
    }
    else if (addr == IRQ_STAT_ADDR)
    {
      //std::cout << "lecture sur I_STAT" << irq.readStatus() <<"\n";
        return irq.readStatus();
    }
    else if (addr == IRQ_MASK_ADDR)
    {
        //std::cout << "lecture sur I_MASK " << irq.readStatus() <<"\n";
        return irq.readStatus();
    }
    else if (addr == GP0_ADDR)
    {
        std::cout << "lecture sur gp0 " << gpu.readData() << "\n";
        return gpu.readData();
    }
    else if (addr == GP1_ADDR)
    {
        std::cout << "lecture sur gp1 " << gpu.readStatus() << "\n";
        return gpu.readStatus();
    }
    else if (addr >= DMA_BASE && addr < DMA_END)
    {
        std::cout << "lecture sur DMA - adresse: 0x" << std::hex << addr << " : valeur: 0x" << std::hex << dma->read(addr) <<"\n";
        return dma->read(addr);
    }
    else if (addr == DPCR_ADDR || addr == DICR_ADDR) {
        std::cout << "lecture sur DMA - adresse: 0x" << std::hex << addr << " : valeur: 0x" << std::hex << dma->read(addr) << "\n";
        return dma->read(addr);
    }
    else {
        std::cerr << "Read32 from unknown address: 0x" << std::hex << addr << "\n";
        return 0;
    }
}

u16 Memoire::load16(u32 addr) {
    addr = mask_region(addr);
    if (addr < RAM_SIZE) {
        return *(u16*)&ram[addr];
    }
    else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
        return *(u16*)&bios[addr - BIOS_START];
    }
    else if (addr == IRQ_STAT_ADDR)
    {
        std::cout << "lecture sur I_STAT" << "\n";
        return irq.readStatus();
    }
    else if (addr == IRQ_MASK_ADDR)
    {
        std::cout << "lecture sur I_MASK" << "\n";
        return irq.readStatus();
    }
    else if (addr >= 0x1F801C00 && addr < 0x1F801E00)
    {
        if (addr == 0x1F801DAE) {  
            u16 spucnt = spu_regs[(0x1F801DAA - 0x1F801C00) >> 1];
            std::cout << "lecture dans le registre SPU: 0x" << std::hex << ((0x1F801DAA - 0x1F801C00) >> 1) << " valeur : 0x" << std::hex <<(spucnt & 0x3F) << "\n";
            return spucnt & 0x3F;
        }
        std::cout << "lecture dans le registre SPU: 0x" << std::hex << ((addr - 0x1F801C00) >> 1) << " valeur : 0x" << std::hex << (spu_regs[(addr - 0x1F801C00) >> 1]) << "\n";
        return spu_regs[(addr - 0x1F801C00) >> 1];

    }
    else {
        std::cerr << "Read16 from unknown address: 0x" << std::hex << addr << "\n";
        return 0;
    }
}

u8 Memoire::load8(u32 addr) {
    addr = mask_region(addr);
    if (addr < RAM_SIZE) {
        return ram[addr];
    }
    else if (addr >= BIOS_START && addr < BIOS_START + BIOS_SIZE) {
        return bios[addr - BIOS_START];
    }
    else if(addr >= 0x1F000000 && addr < 0x1F080000)
    {
        return 0xFF;
    }
    else {
        std::cerr << "Read8 from unknown address: 0x" << std::hex << addr << "\n";
        return 0;
    }
}

void Memoire::store32(u32 addr, u32 value) {
    addr = mask_region(addr);
    if (addr < RAM_SIZE) {
        *(u32*)&ram[addr] = value;
    }
    else if (addr == 0x1F801060)
    {
        std::cout << "ecriture sur RAM_SIZE" << "\n";
    }
    else if (addr == 0xFFFE0130) 
    {
        std::cout << "ecriture sur Cache Control" << "\n";
    }
    else if (addr >= 0x1F801000 && addr <= 0x1F801020)
    {
        std::cout << "ecriture sur Memory control" << "\n";
    }
    else if (addr >= 0x1F801C00 && addr <= 0x1F801FFF)
    {
        std::cout << "ecriture sur SPU" << "\n";
    }
    else if (addr >= 0x1F802000 && addr <= 0x1F80207F)
    {
        std::cout << "ecriture sur Expansion 2 / POST" << "\n";
    }
    else if (addr >= 0x1F801100 && addr <= 0x1F80112F)
    {
        std::cout << "ecriture sur Timers" << "\n";
    }
    else if (addr == IRQ_STAT_ADDR)
    {
        //std::cout << "ecriture sur I_STAT " << value <<"\n";
        irq.writeStatus(value);
        return;
    }
    else if (addr == IRQ_MASK_ADDR)
    {
        //std::cout << "ecriture sur I_MASK - valeur 0x:" << std::hex << value <<"\n";
        irq.writeMask(value);
        return;

    }
    else if (addr == GP0_ADDR)
    {
        std::cout << "ecriture sur gp0: 0x" << value << "\n";
        gpu.gp0(value);
        return;

    }
    else if (addr == GP1_ADDR)
    {
        std::cout << "ecriture sur gp1: 0x" << value << "\n";
        gpu.gp1(value);
        return;

    }
    else if (addr >= DMA_BASE && addr < DMA_END)
    {
        std::cout << "ecriture sur DMA - adresse: 0x" << std::hex << addr << " valeur: 0x" << std::hex << value <<"\n";
        dma->write(addr, value);
    }
    else if (addr == DPCR_ADDR || addr == DICR_ADDR) {
        std::cout << "ecriture sur DMA - adresse: 0x" << std::hex << addr << " valeur: 0x" << std::hex << value << "\n";
        dma->write(addr, value);
    }
    else {
        std::cerr << "Write32 to unknown address: 0x" << std::hex << addr << "\n";
    }
}

void Memoire::store16(u32 addr, u16 value) {
    addr = mask_region(addr);
    if (addr < RAM_SIZE) {
        *(u16*)&ram[addr] = value;
    }
    else if (addr >= 0x1F801C00 && addr <= 0x1F801FFF)
    {
        std::cout << "ecriture sur SPU" << "\n";
    }
    else if (addr >= 0x1F802000 && addr <= 0x1F80207F)
    {
        std::cout << "ecriture sur Expansion 2 / POST" << "\n";
    }
    else if (addr >= 0x1F801100 && addr <= 0x1F80112F)
    {
        std::cout << "ecriture sur Timers" << "\n";
    }
    else if (addr == IRQ_STAT_ADDR)
    {
        //std::cout << "ecriture sur I_STAT" << "\n";
        irq.writeStatus(value);
        return;
    }
    else if (addr == IRQ_MASK_ADDR)
    {
        //std::cout << "ecriture sur I_MASK - valeur 0x:" << std::hex << value << "\n";
        irq.writeMask(value);
        return;

    }
    else if (addr >= 0x1F801C00 && addr < 0x1F801E00) {
        std::cout << "ecriture dans le registre SPU: 0x" << std::hex << ((addr - 0x1F801C00) >> 1) << " valeur : 0x" << std::hex << value <<"\n";
        spu_regs[(addr - 0x1F801C00) >> 1] = value;
        return;
    }
    else {
        std::cerr << "Write16 to unknown address: 0x" << std::hex << addr << "\n";
    }
}

void Memoire::store8(u32 addr, u8 value) {
    addr = mask_region(addr);
    if (addr < RAM_SIZE) {
        ram[addr] = value;
    }
    else if (addr >= 0x1F801C00 && addr <= 0x1F801FFF)
    {
        std::cout << "ecriture sur SPU" << "\n";
    }
    else if (addr >= 0x1F802000 && addr <= 0x1F80207F)
    {
        std::cout << "ecriture sur Expansion 2 / POST" << "\n";
    }
    else if (addr >= 0x1F801100 && addr <= 0x1F80112F)
    {
        std::cout << "ecriture sur Timers" << "\n";
    }
    else if (addr == 0x1F801C00 || addr == 0x1F801FFF)
    {
        //std::cout << "ecriture sur Timers I_STAT / I_MASK" << "\n";
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