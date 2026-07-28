#include "Gpu.h"
#include <iostream>

void Gpu::gp0(u32 value) {
    if (!LOG_GPU) return;   // interrupteur global

    u32 cmd = value >> 24;  // l'octet de poids fort = le type de commande

    // Nommer les grandes familles de commandes GP0
    const char* nom = "?";
    if (cmd == 0x00) nom = "NOP";
    else if (cmd == 0x01) nom = "Clear Cache";
    else if (cmd == 0x02) nom = "Fill Rectangle (VRAM)";      // <-- DESSIN
    else if (cmd >= 0x20 && cmd <= 0x3F) nom = "Polygone";    // <-- DESSIN (triangles/quads)
    else if (cmd >= 0x40 && cmd <= 0x5F) nom = "Ligne";       // <-- DESSIN
    else if (cmd >= 0x60 && cmd <= 0x7F) nom = "Rectangle";   // <-- DESSIN (sprites)
    else if (cmd == 0xA0) nom = "Copy CPU->VRAM";             // <-- CHARGEMENT TEXTURE/LOGO
    else if (cmd == 0xC0) nom = "Copy VRAM->CPU";
    else if (cmd == 0x80) nom = "Copy VRAM->VRAM";
    else if (cmd == 0xE1) nom = "Draw Mode";                  // config
    else if (cmd == 0xE2) nom = "Texture Window";             // config
    else if (cmd == 0xE3) nom = "Drawing Area TL";            // config
    else if (cmd == 0xE4) nom = "Drawing Area BR";            // config
    else if (cmd == 0xE5) nom = "Drawing Offset";             // config
    else if (cmd == 0xE6) nom = "Mask Setting";               // config

    // Marqueur visuel pour les commandes de DESSIN (celles qui nous interessent)
    bool estDessin = (cmd == 0x02) ||
        (cmd >= 0x20 && cmd <= 0x7F) ||
        (cmd == 0xA0) || (cmd == 0x80);

    std::cout << (estDessin ? ">>> DESSIN " : "    config ")
        << "GP0 0x" << std::hex << cmd
        << " (" << nom << ")  mot=0x" << value << "\n";
}

void Gpu::gp1(u32 value) {
    if (!LOG_GPU) return;

    u32 cmd = value >> 24;
    const char* nom = "?";
    if (cmd == 0x00) nom = "Reset GPU";
    else if (cmd == 0x01) nom = "Reset Command Buffer";
    else if (cmd == 0x02) nom = "Ack IRQ";
    else if (cmd == 0x03) nom = "Display Enable";
    else if (cmd == 0x04) nom = "DMA Direction";
    else if (cmd == 0x05) nom = "Display Area Start";
    else if (cmd == 0x06) nom = "Horizontal Display Range";
    else if (cmd == 0x07) nom = "Vertical Display Range";
    else if (cmd == 0x08) nom = "Display Mode";

    std::cout << "    GP1 0x" << std::hex << cmd
        << " (" << nom << ")  mot=0x" << value << "\n";
}