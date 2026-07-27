#include "emu.h"
#include "Memoire.h"
#include "Cpu.h"
#include <iostream>
#include <array>
#include <cstdint>

int main() {
    Memoire mem;
    mem.loadBIOS("../bios/SCPH1001.BIN");
    //Emulator emu;
    //emu.loadTestProgram(); // charge notre mini programme
    //emu.run(20); // exécute 20 instructions
    return 0;
}