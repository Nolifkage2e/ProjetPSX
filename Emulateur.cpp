#include "Emulateur.h"
#include <iostream>

Emulateur::Emulateur() : dma(memoire, gpu), cpu(memoire) {
    memoire.setDma(&dma);   // ? c'est LUI qu'on veut
}

void Emulateur::loadBIOS(const std::string& path) {
    memoire.loadBIOS(path);
    std::cout << "BIOS charge : " << path << "\n";
}

void Emulateur::loadTestProgram() {
    memoire.loadTestProgram();
    std::cout << "Programme de test charge\n";
}

void Emulateur::run(u64 steps) {
    running = true;
    for (u64 i = 0; i < steps && running; ++i) {
        cpu.step();
        if (++vblank_counter >= 564480) {
            vblank_counter = 0;
            memoire.getIrq().request(0);   // lève le bit 0 (VBlank) dans I_STAT
        }
    }
    running = false;
}

void Emulateur::runForever() {
    running = true;
    while (running) {
        cpu.step();
    }
}

void Emulateur::stop() {
    running = false;
}