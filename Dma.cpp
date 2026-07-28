#include "Dma.h"
#include "Memoire.h" 
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
        case 0x8: {
            if (channel == 6 && value == 0x76f4321) {
                std::cerr << "\n!!! ECRITURE PARASITE dans CHCR6 = 0x" << std::hex << value << " !!!\n";
                // On veut savoir QUI a fait ça. Il faut le PC du CPU.
            }
            channels[channel].chcr = value;
            u32 chcr = channels[channel].chcr;
            u32 mode = (chcr >> 9) & 3;

            std::cout << "CHCR canal " << std::dec << channel
                << " = 0x" << std::hex << chcr
                << " (mode " << std::dec << mode << ")\n";

            // Condition de declenchement selon le mode :
            bool trigger;
            bool enabled = (dpcr >> (channel * 4 + 3)) & 1;
            if (!enabled) {
                std::cout << "  -> canal " << channel << " PAS active dans DPCR, ignore\n";
                return;
            }
            if (mode == 0) {
                // SyncMode 0 (manuel/OTC) : besoin de start (24) ET trigger (28)
                trigger = (chcr & 0x01000000) && (chcr & 0x10000000);
            }
            else {
                // SyncMode 1 et 2 : start (24) seul suffit
                trigger = (chcr & 0x01000000);
            }

            if (trigger && enabled) {
                std::cout << "  -> transfert declenche (canal " << std::dec << channel
                    << ", mode " << mode << ")\n";
                if (channel == 6) {
                    std::cout << "  -> OTC\n";
                    transferOTC(channel);
                }
                else if (mode == 2) {
                    std::cout << "  -> Linked list (GPU)\n";
                    transferLinkedList(channel);
                }
                else {
                    std::cout << "  -> mode " << std::dec << mode
                        << " canal " << channel << " : PAS ENCORE IMPLEMENTE\n";
                }
            }
            else {
                std::cout << "  -> pas de declenchement (bit 24/28 manquant)\n";
            }
            return;
        }

                std::cerr << "DMA write inconnu : 0x" << std::hex << addr << "\n";
        }
    }
}

void Dma::transferLinkedList(int channel) {
    u32 addr = channels[channel].madr & 0x1FFFFC;  // masque région + aligne sur 4

    while (true) {
        u32 header = memoire.load32(addr);
        u32 count = header >> 24;          // nombre de mots de commande

        // Envoyer les 'count' mots suivants au GP0
        for (u32 i = 0; i < count; i++) {
            addr = (addr + 4) & 0x1FFFFC;
            u32 command = memoire.load32(addr);
            gpu.gp0(command);               // ← les commandes arrivent enfin au GPU !
        }

        // Passer au nœud suivant
        addr = header & 0x1FFFFF;           // 24 bits bas = pointeur suivant

        // Marqueur de fin : le vrai hardware teste le bit 23
        if (header & 0x800000) break;
    }

    // Transfert termine : effacer les bits busy/start de CHCR
    channels[channel].chcr &= ~0x01000000;  // bit 24
    channels[channel].chcr &= ~0x10000000;  // bit 28
}

void Dma::transferOTC(int channel) {
    u32 addr = channels[channel].madr & 0x1FFFFC;
    u32 count = channels[channel].bcr & 0xFFFF;   // BCR = nombre d'entrées

    for (u32 i = 0; i < count; i++) {
        u32 value;
        if (i == count - 1) {
            value = 0xFFFFFF;              // dernière entrée = marqueur de fin
        }
        else {
            value = (addr - 4) & 0x1FFFFF; // pointe vers l'entrée précédente
        }
        memoire.store32(addr, value);
        addr -= 4;                          // on recule
    }

    // Transfert terminé : effacer busy/start
    channels[channel].chcr &= ~0x01000000;
    channels[channel].chcr &= ~0x10000000;
}
