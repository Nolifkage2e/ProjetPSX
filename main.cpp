#include "Emulateur.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
//#include <SDL.h>  

int main() {
    Emulateur emu;
    emu.loadBIOS("SCPH1001.BIN");
    emu.run(100000000);   // les 1000 premieres instructions du BIOS
    u32 pc = emu.getCpu().getPc();
    std::cout << "PC final : 0x" << std::hex << pc << "\n\n";
    



    for (u32 addr = pc - 0x14; addr <= pc + 0x14; addr += 4) {
        u32 instr = emu.getMemoire().load32(addr);
        std::cout << "0x" << std::hex << addr << " : 0x"
            << std::setw(8) << std::setfill('0') << instr << "\n";
    }

    std::cout << "a2 (compteur) : 0x" << std::hex << emu.getCpu().getReg(6) << "\n";

    return 0;
}