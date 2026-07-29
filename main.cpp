#include "Emulateur.h"
#include "Ecran.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <SDL.h>  

int main(int argc, char* argv[]) {
    Emulateur emu;
    emu.loadBIOS("SCPH1001.BIN");
    Ecran ecran;
    //emu.run(100000000);   // les 1000 premieres instructions du BIOS
    //u32 pc = emu.getCpu().getPc();
    //std::cout << "PC final : 0x" << std::hex << pc << "\n\n";
    



    //for (u32 addr = pc - 0x14; addr <= pc + 0x14; addr += 4) {
     //   u32 instr = emu.getMemoire().load32(addr);
       // std::cout << "0x" << std::hex << addr << " : 0x"
      //      << std::setw(8) << std::setfill('0') << instr << "\n";
   // }

    //std::cout << "a2 (compteur) : 0x" << std::hex << emu.getCpu().getReg(6) << "\n";

    
    if (!ecran.init()) return 1;

    // --- TEST : remplir la VRAM d'un degrade pour valider l'affichage ---
    // (a retirer une fois que le vrai rendu GP0 fonctionne)
    //emu.getGpu().fillTestPattern();
    //emu.getGpu().fillTestPattern();
    // --- Boucle principale ---
    bool running = true;
    //emu.run(27700000);
    //u32 pc = emu.getCpu().getPc();
    //std::cout << "\nPC : 0x" << std::hex << pc << "\n";
    //for (u32 a = pc - 0x20; a <= pc + 0x20; a += 4)
        //std::cout << "0x" << a << " : 0x" << std::setw(8) << std::setfill('0')
        //<< emu.getMemoire().load32(a) << "\n";

    
    while (running) {
        // 1. Emuler ~une frame d'instructions (approx. 1/60 s de CPU)
   
        emu.run(564480);                    // petits paquets
        if (emu.frameePrete()) {           // affiche seulement quand une frame est finie
            ecran.afficherVram(emu.getGpu().getVram());
            emu.resetFramePrete();
        }
        running = ecran.gererEvenements();
    }

    return 0;
}