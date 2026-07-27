#include "Emulateur.h"
#include <iostream>

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