#pragma once

#include "common.h"
#include "Memoire.h"
#include "Dma.h"
#include "Cpu.h"
#include <string>

class Emulateur {
    Memoire memoire;
    Gpu     gpu;
    Dma     dma;
    CPU cpu;

    bool running = false;

    u64 vblank_counter = 0;

    bool frame_prete = false;

public:
    Emulateur();
    Dma& getDma() { return dma; }

    // Chargement
    void loadBIOS(const std::string& path);
    void loadTestProgram();

    // Execution
    void run(u64 steps);   // execute un nombre fixe d'instructions
    void runForever();     // boucle jusqu'a stop() (Ctrl+C pour quitter)
    void stop();
    bool frameePrete();
    void resetFramePrete();

    

    // Acces pour le debug futur (desassembleur, breakpoints...)
    Memoire& getMemoire() { return memoire; }
    CPU& getCpu() { return cpu; }
    Gpu& getGpu() { return gpu; }
};