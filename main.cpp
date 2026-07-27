#include "Emulateur.h"

int main() {
    Emulateur emu;

    emu.loadBIOS("SCPH1001.BIN");
    emu.run(5000);   // les 1000 premieres instructions du BIOS

    return 0;
}