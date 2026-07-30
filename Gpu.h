// Gpu.h
#pragma once
#include "common.h"
#include <array>
#include <vector>

constexpr u32 GP0_ADDR = 0x1F801810;  // écriture commandes / lecture GPUREAD
constexpr u32 GP1_ADDR = 0x1F801814;  // écriture contrôle / lecture GPUSTAT

constexpr int VRAM_WIDTH = 1024;
constexpr int VRAM_HEIGHT = 512;

class Gpu {
    bool LOG_GPU = true;
    std::vector<u16> vram;

    std::array<u32, 16> gp0_buffer{};  // parametres accumules
    u32 gp0_count = 0;                  // combien de mots recus
    u32 gp0_needed = 0;                 // combien de mots attendus au total
    u32 gp0_command = 0;

    s16 draw_offset_x = 0;
    s16 draw_offset_y = 0;

    u32 gpu_status = 0x14000000; // Statut par défaut (Ready, Display Off)

    u32 scanline = 0;

    // Coordonnées du coin supérieur gauche de l'écran dans la VRAM
    u16 display_vram_x = 0;
    u16 display_vram_y = 0;

    u16 texpage_x = 0;   // coin X de la page de texture en VRAM (multiple de 64)
    u16 texpage_y = 0;   // coin Y de la page (0 ou 256)
    u8  tex_depth = 0;   // 0 = 4bit CLUT, 1 = 8bit CLUT, 2 = 15bit direct
    u16 clut_x = 0;   // position de la palette (CLUT) en VRAM
    u16 clut_y = 0;

    bool gp0_transfer = false;  // sommes-nous en train de recevoir des pixels ?
    int  transfer_x = 0, transfer_y = 0;
    int  transfer_w = 0, transfer_h = 0;
    int  transfer_cx = 0, transfer_cy = 0;// position courante dans la zone

    u32 transfer_words_total = 0;
    u32 transfer_words_recus = 0;

    // --- Rasterizer (dessin dans la VRAM) ---
    void fillRectangle(u32 color, u16 x, u16 y, u16 w, u16 h);
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, u16 color);

    // Convertit une couleur 24 bits (0xBBGGRR) en 16 bits BGR555
    static u16 to555(u32 color24);
    static u32 calculateGp0Needed(u32 command_word);

    u16 lireTexel(int u, int v);

    // Rasterise un triangle TEXTURE (interpolation barycentrique des u,v)
    void drawTriangleTexture(int x0, int y0, int u0, int v0,
        int x1, int y1, int u1, int v1,
        int x2, int y2, int u2, int v2);

    void drawTriangleGouraudTexture(
        int x0, int y0, int u0, int v0, u32 c0,
        int x1, int y1, int u1, int v1, u32 c1,
        int x2, int y2, int u2, int v2, u32 c2);

    void drawTriangleGouraud(int x0, int y0, u32 c0, int x1, int y1, u32 c1, int x2, int y2, u32 c2);

    void drawRectTexture(int x, int y, int w, int h,
        int u, int v, u32 couleur, bool is_raw);


public:
    Gpu() : vram(VRAM_WIDTH* VRAM_HEIGHT, 0) {}

    void tick(u32 cycles) { scanline = (scanline + cycles / 2160) % 263; }

    u32 readStatus();

    u32 readData();

    void gp0(u32 value);
    void gp1(u32 value);

    const u16* getVram() const { return vram.data(); }

    // Helpers de dessin (rasterizer) - a etoffer
    void vramSet(int x, int y, u16 color);
    u16  vramGet(int x, int y) const;

    // Test : remplir la VRAM d'un degrade pour valider l'affichage
    void fillTestPattern();

    bool estEnTransfert() const { return gp0_transfer; }
    u32  motsRestants()   const { return gp0_transfer ? (transfer_words_total - transfer_words_recus) : 0; }
};

