#include "Emulateur.h"
#include <iostream>

Emulateur::Emulateur() : dma(memoire, gpu), cpu(memoire) {
    memoire.setDma(&dma);
    memoire.setBreakpoints(&breakpoints);
    cpu.setBreakpoints(&breakpoints);
    // ? c'est LUI qu'on veut
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
        if (breakpoints.estDeclenche()) break;
        if (++vblank_counter >= 564480) {
            vblank_counter = 0;
            memoire.getIrq().request(0);
            frame_prete = true;      // ? signale qu'une frame est complète
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

bool Emulateur::frameePrete() 
{
    return frame_prete;
}

void Emulateur::resetFramePrete() 
{
    frame_prete = false;
}