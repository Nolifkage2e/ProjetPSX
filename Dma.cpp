#include "Dma.h"
#include <iostream>

u32 Dma::read(u32 addr) {
    // Registres globaux
    if (addr == DPCR_ADDR) return dpcr;
    if (addr == DICR_ADDR) return dicr;

    // Registres des canaux : on isole le numero de canal et le registre.
    // offset dans la plage des canaux (0x00 a 0x6F)
    u32 offset = addr - DMA_BASE;
    u32 channel = offset >> 4;         // / 0x10  -> numero de canal (0-6)
    u32 reg = offset & 0xF;        // 0x0=MADR, 0x4=BCR, 0x8=CHCR

    if (channel < 7) {
        switch (reg) {
        case 0x0: return channels[channel].madr;
        case 0x4: return channels[channel].bcr;
        case 0x8: return channels[channel].chcr;
        }
    }

    std::cerr << "DMA read inconnu : 0x" << std::hex << addr << "\n";
    return 0;
}

void Dma::write(u32 addr, u32 value) {
    // Registres globaux
    if (addr == DPCR_ADDR) { dpcr = value; return; }
    if (addr == DICR_ADDR) { dicr = value; return; }

    u32 offset = addr - DMA_BASE;
    u32 channel = offset >> 4;
    u32 reg = offset & 0xF;

    if (channel < 7) {
        switch (reg) {
        case 0x0: channels[channel].madr = value; return;
        case 0x4: channels[channel].bcr = value; return;
        case 0x8:
            channels[channel].chcr = value;
            // TODO (etape suivante) : si le bit de declenchement est actif,
            // lancer le transfert ici. Pour l'instant on stocke seulement.
            return;
        }
    }

    std::cerr << "DMA write inconnu : 0x" << std::hex << addr << "\n";
}
