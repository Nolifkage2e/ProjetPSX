#pragma once

#include "common.h"
#include "Memoire.h"
#include "Cpu.h"
#include <string>

class Emulateur {
    Memoire memoire;
    CPU cpu;

    bool running = false;

public:
    Emulateur() : cpu(memoire) {}

    // Chargement
    void loadBIOS(const std::string& path);
    void loadTestProgram();

    // Execution
    void run(u64 steps);   // execute un nombre fixe d'instructions
    void runForever();     // boucle jusqu'a stop() (Ctrl+C pour quitter)
    void stop();

    // Acces pour le debug futur (desassembleur, breakpoints...)
    Memoire& getMemoire() { return memoire; }
    CPU& getCpu() { return cpu; }
};