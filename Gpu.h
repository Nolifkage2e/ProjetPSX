// Gpu.h
#pragma once
#include "common.h"

constexpr u32 GP0_ADDR = 0x1F801810;  // écriture commandes / lecture GPUREAD
constexpr u32 GP1_ADDR = 0x1F801814;  // écriture contrôle / lecture GPUSTAT

class Gpu {
public:
    u32 readStatus() {
        // Bits 26-28 à 1 = prêt à tout recevoir.
        // Bit 31 (DMA/CPU ready) souvent testé aussi.
        return 0x1C000000;
    }

    u32 readData() { return 0; }  // GPUREAD, on verra plus tard

    void gp0(u32 value) { /* commandes de rendu, à implémenter */ }
    void gp1(u32 value) { /* commandes de contrôle, à implémenter */ }
};

