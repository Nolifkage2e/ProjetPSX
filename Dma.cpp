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
    std::cout << "reg: 0x" << reg << "\n";

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
    if (addr == DPCR_ADDR) { dpcr = value; std::cout << "activation dma" << "\n"; return; }
    if (addr == DICR_ADDR) { dicr = value; return; }

    u32 offset = addr - DMA_BASE;
    u32 channel = offset >> 4;
    u32 reg = offset & 0xF;
    std::cout << "reg: 0x" << reg << "\n";

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
                else if (mode == 0) {
                    std::cout << "  -> Mode 0 (Immédiat)\n";
                    transferImmediate(channel);
                }
                else if (mode == 1) {
                    std::cout << "  -> Block transfer (canal " << channel << ")\n";
                    transferBlock(channel);
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
    int securite = 0;
    while (true) {
        if (++securite > 20000) break;

        if (addr >= RAM_SIZE) {       // hors RAM = liste corrompue
            std::cerr << "LL addr invalide 0x" << std::hex << addr << "\n";
            break;
        }

        u32 header = memoire.load32(addr);
        u32 count = header >> 24;

        // Garde-fou : un count énorme est suspect
        if (count > 16) {
            std::cerr << "LL count suspect = " << std::dec << count
                << " (header=0x" << std::hex << header << ")\n";
        }

        for (u32 i = 0; i < count; i++) {
            addr = (addr + 4) & 0x1FFFFC;
            gpu.gp0(memoire.load32(addr));
        }

        if (header & 0x800000) break;
        addr = header & 0x1FFFFC;
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

void Dma::transferBlock(int channel) {
    channels[channel].chcr &= ~0x01000000;
    channels[channel].chcr &= ~0x10000000;
    return;

    u32 chcr = channels[channel].chcr;
    u32 bcr = channels[channel].bcr;

    u32 block_size = bcr & 0xFFFF;
    u32 block_count = (bcr >> 16) & 0xFFFF;

    u32 mode = (chcr >> 9) & 3;
    u32 total_words;

    if (mode == 1) {
        // SyncMode 1 : total = block_size * block_count.
        // Un compte de 0 signifie "aucun bloc", pas 65536 !
        if (block_count == 0) {
            channels[channel].chcr &= ~0x01000000;
            channels[channel].chcr &= ~0x10000000;
            return;
        }
        total_words = block_size * block_count;
    }
    else {
        // SyncMode 0 : un seul bloc, et la règle "0 = 0x10000" s'applique
        total_words = (block_size == 0) ? 0x10000 : block_size;
    }

    // Garde-fou : un transfert absurde est refuse
    if (total_words > 0x40000) {   // > 256k mots = 1 Mo, impossible
        std::cerr << "BLOCK suspect : " << std::dec << total_words
            << " mots (BCR=0x" << std::hex << bcr << "), ignore\n";
        channels[channel].chcr &= ~0x01000000;
        channels[channel].chcr &= ~0x10000000;
        return;
    }

    u32 addr = channels[channel].madr & 0x1FFFFC;
    bool from_ram = (chcr & 1);
    int step = ((chcr >> 1) & 1) ? -4 : +4;

    std::cout << "BLOCK canal " << std::dec << channel
        << " MADR=0x" << std::hex << channels[channel].madr
        << " BCR=0x" << bcr
        << " (size=" << std::dec << block_size
        << " count=" << block_count
        << " total=" << total_words << ")"
        << " dir=" << (chcr & 1) << "\n";
    for (int k = 0; k < 4 && k < (int)total_words; k++) {
        std::cout << "   mot[" << k << "] = 0x" << std::hex
            << memoire.load32((addr + k * 4) & 0x1FFFFC) << "\n";
    }

    std::cout << "   (GPU en transfert=" << gpu.estEnTransfert()
        << ", attend encore " << std::dec << gpu.motsRestants() << " mots)\n";



    for (u32 i = 0; i < total_words; i++) {
        if (from_ram) {
            u32 data = memoire.load32(addr);
            if (channel == 2) gpu.gp0(data);
            // autres canaux à venir
        }
        else {
            u32 data = 0;
            if (channel == 2) data = gpu.readData();
            memoire.store32(addr, data);
        }
        addr = (addr + step) & 0x1FFFFC;
    }

    channels[channel].madr = addr;
    channels[channel].bcr &= 0x0000FFFF;
    channels[channel].chcr &= ~0x01000000;
    channels[channel].chcr &= ~0x10000000;

    
}

void Dma::transferImmediate(int channel) {
    u32 chcr = channels[channel].chcr;

    // En Mode 0, le nombre de mots à transférer est dans les 16 bits bas de BCR
    u32 count = channels[channel].bcr & 0xFFFF;

    // Cas particulier : 0 signifie 65 536 mots (0x10000)
    if (count == 0) count = 0x10000;

    // Adresse de départ dans la RAM (masquée et alignée sur 32 bits)
    u32 addr = channels[channel].madr & 0x1FFFFC;

    // Bit 0 de CHCR : Direction (0 = Périphérique -> RAM, 1 = RAM -> Périphérique)
    bool from_ram = (chcr & 1);

    // Bit 1 de CHCR : Pas de l'adresse (0 = +4 octets, 1 = -4 octets)
    int step = ((chcr >> 1) & 1) ? -4 : +4;

    for (u32 i = 0; i < count; i++) {
        if (from_ram) {
            // --- RAM vers Périphérique ---
            u32 data = memoire.load32(addr);

            switch (channel) {
            case 0: // MDEC in (envoi de données compressées)
                // mdec.write(data);
                break;
            case 2: // GPU (envoi de polygones / commandes)
                gpu.gp0(data);
                break;
            case 4: // SPU (envoi de samples audio)
                // spu.write(data);
                break;
            default:
                std::cerr << "DMA Mode 0 RAM->Dev non géré sur canal " << channel << "\n";
                break;
            }
        }
        else {
            // --- Périphérique vers RAM ---
            u32 data = 0;

            switch (channel) {
            case 1: // MDEC out (récupération de l'image décompressée)
                // data = mdec.read();
                break;
            case 2: // GPU (lecture directe VRAM)
                data = gpu.readData();
                break;
            case 3: // CD-ROM (lecture brute)
                // data = cdrom.readFIFO();
                break;
            default:
                std::cerr << "DMA Mode 0 Dev->RAM non géré sur canal " << channel << "\n";
                break;
            }

            memoire.store32(addr, data);
        }

        // Avancer ou reculer l'adresse selon le bit de direction
        addr = (addr + step) & 0x1FFFFC;
    }

    // Mise à jour de MADR avec l'adresse finale atteinte
    channels[channel].madr = addr;

    // En mode 0, le compteur BCR retombe à 0 après le transfert
    channels[channel].bcr &= 0xFFFF0000;

    // Transfert terminé : effacer le bit Busy (24) ET le bit Trigger (28)
    channels[channel].chcr &= ~0x01000000; // Bit 24
    channels[channel].chcr &= ~0x10000000; // Bit 28
}
