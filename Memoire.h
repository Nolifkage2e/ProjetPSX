#pragma once

#include "common.h"
#include <array>
#include <string>
#include <vector>

// Constantes memoire
constexpr size_t RAM_SIZE = 2 * 1024 * 1024;  // 2 MB
constexpr size_t BIOS_SIZE = 512 * 1024;       // 512 KB
constexpr u32    BIOS_START = 0x1FC00000;

class Memoire {
    std::vector<u8> ram;
    std::vector<u8> bios;

public:
    Memoire() : ram(RAM_SIZE, 0), bios(BIOS_SIZE, 0) {}
    // Lectures : l'adresse est TOUJOURS sur 32 bits,
    // seule la taille de la donnee change.
    u32 load32(u32 addr);
    u16 load16(u32 addr);
    u8  load8(u32 addr);

    // Ecritures
    void store32(u32 addr, u32 value);
    void store16(u32 addr, u16 value);
    void store8(u32 addr, u8  value);

    // Chargement
    void loadBIOS(const std::string& filename);
    void loadTestProgram();
};