#pragma once

#include "common.h"
//#include "Memoire.h"
#include "Gpu.h"
#include <array>

// Controleur DMA de la PS1.
//
// 7 canaux, chacun avec 3 registres 32 bits a partir de 0x1F801080 :
//   canal n : base = 0x1F801080 + n*0x10
//     +0  MADR  (Memory Address)  : adresse RAM de depart
//     +4  BCR   (Block Control)   : taille du transfert
//     +8  CHCR  (Channel Control) : direction, mode de synchro, declenchement
//
// 2 registres globaux :
//   0x1F8010F0  DPCR (Control)   : activation/priorite des canaux
//   0x1F8010F4  DICR (Interrupt) : gestion des interruptions DMA
//
// Canaux : 0=MDECin 1=MDECout 2=GPU 3=CDROM 4=SPU 5=PIO 6=OTC

class Memoire;

constexpr u32 DMA_BASE = 0x1F801080;
constexpr u32 DMA_END = 0x1F801100;   // fin de la plage (exclue)

constexpr u32 DPCR_ADDR = 0x1F8010F0;
constexpr u32 DICR_ADDR = 0x1F8010F4;

// Un canal individuel : juste ses 3 registres pour l'instant.
struct DmaChannel {
    u32 madr = 0;   // adresse memoire
    u32 bcr = 0;   // block control
    u32 chcr = 0;   // channel control
};

class Dma {
    std::array<DmaChannel, 7> channels{};
    Memoire& memoire;
    Gpu& gpu;
    

    // Registres globaux. DPCR a une valeur de reset non nulle sur le
    // vrai hardware (0x07654321), le BIOS s'attend a la relire.
    u32 dpcr = 0x07654321;
    u32 dicr = 0;

public:
    Dma(Memoire& mem, Gpu& g) : memoire(mem), gpu(g) {}
    // Lecture d'un registre DMA (adresse deja masquee par Memoire).
    u32 read(u32 addr);

    // Ecriture d'un registre DMA.
    void write(u32 addr, u32 value);

    void transferLinkedList(int channel);
    void transferOTC(int channel);
};
