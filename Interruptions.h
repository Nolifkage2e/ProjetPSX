#pragma once

#include "common.h"

// Controleur d'interruptions de la PS1
//
//   0x1F801070  I_STAT : interruptions en attente (1 = declenchee)
//   0x1F801074  I_MASK : interruptions autorisees (1 = ecoutee)
//
// Les 11 sources d'interruption (bits de I_STAT/I_MASK) :
//   bit 0  VBLANK    bit 4  DMA       bit 8   SIO
//   bit 1  GPU       bit 5  Timer0    bit 9   SPU
//   bit 2  CDROM     bit 6  Timer1    bit 10  Lightpen/PIO
//   bit 3  DMA?      bit 7  Timer2
// (voir psx-spx pour le detail exact)

constexpr u32 IRQ_STAT_ADDR = 0x1F801070;
constexpr u32 IRQ_MASK_ADDR = 0x1F801074;

class Interruptions {
    u32 status = 0;   // I_STAT
    u32 mask = 0;   // I_MASK

public:
    // --- Acces depuis le bus (Memoire) ---

    u32 readStatus() const { return status; }
    u32 readMask()   const { return mask; }

    // Ecrire dans I_STAT sert a ACQUITTER :
    // le programme ecrit un masque avec des 0 sur les bits a effacer.
    // status = status AND value (on ne peut PAS declencher une
    // interruption en ecrivant un 1, seulement en effacer).
    void writeStatus(u32 value) { status &= value; }

    void writeMask(u32 value) { mask = value; }

    // --- Acces depuis les peripheriques (plus tard : GPU, timers...) ---

    // Un peripherique declenche une interruption en levant son bit.
    void request(u32 irq_bit) { status |= (1u << irq_bit); }

    // Le CPU verifiera ceci pour savoir s'il doit prendre l'exception
    // (combine avec les bits d'activation du COP0 SR, plus tard).
    bool pending() const { return (status & mask) != 0; }
};
