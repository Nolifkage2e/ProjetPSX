#pragma once

#include "common.h"
#include "Memoire.h"
#include "Dma.h"
#include "Cpu.h"
#include "Breakpoints.h"
#include <string>

class Emulateur {
    Memoire memoire;
    Gpu     gpu;
    Dma     dma;
    CPU cpu;
    Breakpoints breakpoints;


    bool running = false;

    u64 vblank_counter = 0;

    bool frame_prete = false;

public:
    Breakpoints& getBreakpoints() { return breakpoints; }
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