#include "Emulateur.h"
#include "Debogueur.h"
#include <SDL.h>

int main(int argc, char* argv[]) {
    Emulateur emu;
    emu.loadBIOS("SCPH1001.BIN");

    Debogueur debug(emu);
    if (!debug.init()) return 1;

    bool running = true;
    while (running) {
        // --- Evenements (ImGui + fermeture) ---
        running = debug.gererEvenements();

        // --- Emulation ---
        if (!debug.estEnPause()) {
            emu.run(debug.getInstructionsParFrame());
            if (emu.getBreakpoints().estDeclenche()) {
                debug.mettreEnPause();     // ← à ajouter dans Debogueur
            }
        }
        else if (debug.consommerStep()) {
            emu.run(1);          // une seule instruction
        }

        // --- Interface ---
        debug.nouvelleFrame();
        debug.dessiner();
        debug.presenter();
    }

    return 0;
}