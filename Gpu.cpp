#include "Gpu.h"
#include <iostream>
#include <iomanip>
#include <algorithm>


#define LOG_HEX(val) "0x" << std::hex << std::setw(8) << std::setfill('0') << (val) << std::dec

void Gpu::vramSet(int x, int y, u16 color) {
    if (x < 0 || x >= VRAM_WIDTH || y < 0 || y >= VRAM_HEIGHT) return;
    vram[y * VRAM_WIDTH + x] = color;
}

u16 Gpu::vramGet(int x, int y) const {
    if (x < 0 || x >= VRAM_WIDTH || y < 0 || y >= VRAM_HEIGHT) return 0;
    return vram[y * VRAM_WIDTH + x];
}

u16 Gpu::to555(u32 color24) {
    u8 r = (color24 >> 0) & 0xFF;
    u8 g = (color24 >> 8) & 0xFF;
    u8 b = (color24 >> 16) & 0xFF;
    return ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3);
}

static inline int parseCoordX(u32 word) {
    return (s32)(word << 21) >> 21;
}

static inline int parseCoordY(u32 word) {
    return (s32)((word >> 16) << 21) >> 21;
}

static inline int texU(u32 word) { return  word & 0xFF; }

static inline int texV(u32 word) { return (word >> 8) & 0xFF; }

void Gpu::fillRectangle(u32 color, u16 x, u16 y, u16 w, u16 h) {
    if (w == 0 || h == 0) return;

    std::cout << " [GPU EXEC] FillRectangle -> Pos(" << x << ", " << y
        << ") Taille(" << w << "x" << h
        << ") Couleur 24bit=" << LOG_HEX(color) << "\n";

    u16 c = to555(color);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            vramSet(x + i, y + j, c);
        }
    }
}

void Gpu::drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, u16 color) {
    int rawMinX = std::min({ x0, x1, x2 });
    int rawMaxX = std::max({ x0, x1, x2 });
    int rawMinY = std::min({ y0, y1, y2 });
    int rawMaxY = std::max({ y0, y1, y2 });

    // Rejet si le triangle est complètement en dehors de la VRAM
    if (rawMaxX < 0 || rawMinX >= VRAM_WIDTH || rawMaxY < 0 || rawMinY >= VRAM_HEIGHT) {
        return;
    }

    int minX = std::max(0, rawMinX);
    int maxX = std::min(VRAM_WIDTH - 1, rawMaxX);
    int minY = std::max(0, rawMinY);
    int maxY = std::min(VRAM_HEIGHT - 1, rawMaxY);

    auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        };

    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    int pixels_dessines = 0;
    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            int w0 = edge(x1, y1, x2, y2, px, py);
            int w1 = edge(x2, y2, x0, y0, px, py);
            int w2 = edge(x0, y0, x1, y1, px, py);

            bool inside = (area > 0)
                ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                : (w0 <= 0 && w1 <= 0 && w2 <= 0);

            if (inside) {
                vramSet(px, py, color);
                pixels_dessines++;
            }
        }
    }
}

void Gpu::drawTriangleTexture(int x0, int y0, int u0, int v0,
    int x1, int y1, int u1, int v1,
    int x2, int y2, int u2, int v2) {
    std::cout << "TEX texpage=(" << std::dec << texpage_x << "," << texpage_y
        << ") depth=" << (int)tex_depth
        << " clut=(" << clut_x << "," << clut_y << ")"
        << " uv0=(" << u0 << "," << v0 << ")\n";
    // Bounding box, limitee a la VRAM
    int rawMinX = std::min({ x0, x1, x2 });
    int rawMaxX = std::max({ x0, x1, x2 });
    int rawMinY = std::min({ y0, y1, y2 });
    int rawMaxY = std::max({ y0, y1, y2 });

    if (rawMaxX < 0 || rawMinX >= VRAM_WIDTH || rawMaxY < 0 || rawMinY >= VRAM_HEIGHT)
        return;

    int minX = std::max(0, rawMinX);
    int maxX = std::min(VRAM_WIDTH - 1, rawMaxX);
    int minY = std::max(0, rawMinY);
    int maxY = std::min(VRAM_HEIGHT - 1, rawMaxY);

    auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        };

    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;   // triangle degenere

    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            // Les 3 edge functions = les coordonnees barycentriques (non normalisees)
            int w0 = edge(x1, y1, x2, y2, px, py);
            int w1 = edge(x2, y2, x0, y0, px, py);
            int w2 = edge(x0, y0, x1, y1, px, py);

            bool inside = (area > 0)
                ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;

            // --- INTERPOLATION BARYCENTRIQUE des coordonnees de texture ---
            // u = (w0*u0 + w1*u1 + w2*u2) / area    (tout en entiers)
            int u = (w0 * u0 + w1 * u1 + w2 * u2) / area;
            int v = (w0 * v0 + w1 * v1 + w2 * v2) / area;

            u16 texel = lireTexel(u, v);

            // Sur PS1, la couleur 0x0000 est TRANSPARENTE : on ne dessine pas
            if (texel == 0) continue;

            vramSet(px, py, texel);
        }
    }
}

void Gpu::drawTriangleGouraudTexture(
    int x0, int y0, int u0, int v0, u32 c0,
    int x1, int y1, int u1, int v1, u32 c1,
    int x2, int y2, int u2, int v2, u32 c2) {

    int rawMinX = std::min({ x0, x1, x2 });
    int rawMaxX = std::max({ x0, x1, x2 });
    int rawMinY = std::min({ y0, y1, y2 });
    int rawMaxY = std::max({ y0, y1, y2 });

    if (rawMaxX < 0 || rawMinX >= VRAM_WIDTH || rawMaxY < 0 || rawMinY >= VRAM_HEIGHT)
        return;

    int minX = std::max(0, rawMinX);
    int maxX = std::min(VRAM_WIDTH - 1, rawMaxX);
    int minY = std::max(0, rawMinY);
    int maxY = std::min(VRAM_HEIGHT - 1, rawMaxY);

    auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        };

    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    // Décomposition des couleurs (0-255)
    int r0 = c0 & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = (c0 >> 16) & 0xFF;
    int r1 = c1 & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF;
    int r2 = c2 & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF;

    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            int w0 = edge(x1, y1, x2, y2, px, py);
            int w1 = edge(x2, y2, x0, y0, px, py);
            int w2 = edge(x0, y0, x1, y1, px, py);

            bool inside = (area > 0)
                ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                : (w0 <= 0 && w1 <= 0 && w2 <= 0);

            if (!inside) continue;

            // Interpolation Barycentrique des UV
            int u = (w0 * u0 + w1 * u1 + w2 * u2) / area;
            int v = (w0 * v0 + w1 * v1 + w2 * v2) / area;

            // Interpolation Barycentrique de la couleur Gouraud
            int r = (w0 * r0 + w1 * r1 + w2 * r2) / area;
            int g = (w0 * g0 + w1 * g1 + w2 * g2) / area;
            int b = (w0 * b0 + w1 * b1 + w2 * b2) / area;

            // Lecture du texel dans la VRAM
            u16 texel = lireTexel(u, v);
            if (texel == 0) continue; // Transparence PS1

            // Extraction RGB du texel 15-bit (et reconversion vers 0-255 en décalant de 3)
            int tex_r = (texel & 0x1F) << 3;
            int tex_g = ((texel >> 5) & 0x1F) << 3;
            int tex_b = ((texel >> 10) & 0x1F) << 3;

            // Modulation PS1 : 128 (0x80) est la valeur neutre
            // Division par 128 optimisée par un bitshift >> 7
            int final_r = std::min(255, (tex_r * r) >> 7);
            int final_g = std::min(255, (tex_g * g) >> 7);
            int final_b = std::min(255, (tex_b * b) >> 7);

            // Reconversion finale en 15-bit (555) pour écrire dans la VRAM
            u16 final_color = (final_r >> 3) | ((final_g >> 3) << 5) | ((final_b >> 3) << 10);

            // Note: Bit 15 = STP (Semi-Transparency). 
            // On conserve celui du texel pour la gestion de l'alpha blending plus tard.
            final_color |= (texel & 0x8000);

            vramSet(px, py, final_color);
        }
    }
}

void Gpu::drawTriangleGouraud(int x0, int y0, u32 c0, int x1, int y1, u32 c1, int x2, int y2, u32 c2) {
    int rawMinX = std::min({ x0, x1, x2 });
    int rawMaxX = std::max({ x0, x1, x2 });
    int rawMinY = std::min({ y0, y1, y2 });
    int rawMaxY = std::max({ y0, y1, y2 });

    if (rawMaxX < 0 || rawMinX >= VRAM_WIDTH || rawMaxY < 0 || rawMinY >= VRAM_HEIGHT)
        return;

    int minX = std::max(0, rawMinX);
    int maxX = std::min(VRAM_WIDTH - 1, rawMaxX);
    int minY = std::max(0, rawMinY);
    int maxY = std::min(VRAM_HEIGHT - 1, rawMaxY);

    auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
        };

    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;

    // Décomposition des couleurs 24-bit en RGB (0-255)
    int r0 = c0 & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = (c0 >> 16) & 0xFF;
    int r1 = c1 & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF;
    int r2 = c2 & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF;

    for (int py = minY; py <= maxY; py++) {
        for (int px = minX; px <= maxX; px++) {
            int w0 = edge(x1, y1, x2, y2, px, py);
            int w1 = edge(x2, y2, x0, y0, px, py);
            int w2 = edge(x0, y0, x1, y1, px, py);

            bool inside = (area > 0)
                ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                : (w0 <= 0 && w1 <= 0 && w2 <= 0);

            if (!inside) continue;

            // Interpolation barycentrique des couleurs
            int r = (w0 * r0 + w1 * r1 + w2 * r2) / area;
            int g = (w0 * g0 + w1 * g1 + w2 * g2) / area;
            int b = (w0 * b0 + w1 * b1 + w2 * b2) / area;

            // Sécurité bornes RGB
            r = std::max(0, std::min(r, 255));
            g = std::max(0, std::min(g, 255));
            b = std::max(0, std::min(b, 255));

            // Conversion en 15-bit (555)
            u16 color555 = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10);

            vramSet(px, py, color555);
        }
    }
}

void Gpu::drawRectTexture(int x, int y, int w, int h,
    int u, int v, u32 couleur, bool is_raw) {
    // Modulation par la couleur de commande (sauf si "raw texture")
    int mod_r = couleur & 0xFF;
    int mod_g = (couleur >> 8) & 0xFF;
    int mod_b = (couleur >> 16) & 0xFF;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            u16 texel = lireTexel(u + i, v + j);

            // 0x0000 = transparent sur PS1
            if (texel == 0) continue;

            u16 final_color = texel;

            if (!is_raw) {
                // Modulation : la couleur de commande teinte le texel.
                // 0x80 (128) est la valeur neutre -> division par 128 = >> 7
                int tr = ((texel & 0x1F) << 3);
                int tg = (((texel >> 5) & 0x1F) << 3);
                int tb = (((texel >> 10) & 0x1F) << 3);

                int fr = std::min(255, (tr * mod_r) >> 7);
                int fg = std::min(255, (tg * mod_g) >> 7);
                int fb = std::min(255, (tb * mod_b) >> 7);

                final_color = (fr >> 3) | ((fg >> 3) << 5) | ((fb >> 3) << 10);
                final_color |= (texel & 0x8000);   // conserve le bit STP
            }

            vramSet(x + i, y + j, final_color);
        }
    }
}

u32 Gpu::calculateGp0Needed(u32 command_word) {
    u8 cmd = command_word >> 24;

    // Polygones (0x20 - 0x3F)
    if ((cmd & 0xE0) == 0x20) {
        bool is_gouraud = (cmd >> 4) & 1;
        bool is_quad = (cmd >> 3) & 1;
        bool is_textured = (cmd >> 2) & 1;

        u32 num_vertices = is_quad ? 4 : 3;
        u32 words_per_vertex = 1;
        if (is_textured) words_per_vertex += 1;
        if (is_gouraud)  words_per_vertex += 1;

        return is_gouraud ? (num_vertices * words_per_vertex)
            : (1 + num_vertices * words_per_vertex);
    }

    // Rectangles / Sprites (0x60 - 0x7F)
    if ((cmd & 0xE0) == 0x60) {
        bool is_textured = (cmd >> 2) & 1;
        u32 size_code = (cmd >> 3) & 3;

        u32 length = 2;
        if (is_textured)  length += 1;
        if (size_code == 0) length += 1;

        return length;
    }

    // Lignes simples (0x40 & 0x50)
    if ((cmd & 0xE0) == 0x40) {
        bool is_polyline = (cmd >> 3) & 1; // Bit 27
        bool is_gouraud = (cmd >> 4) & 1; // Bit 28

        // Pour l'instant, traitez les lignes simples
        if (!is_polyline) {
            return is_gouraud ? 4 : 3;
        }
        return 3; // Fallback temporaire poly-ligne
    }



    // Commandes fixes
    if (cmd == 0x02) return 3; // Fill
    if (cmd == 0x80) return 4; // Copy VRAM->VRAM
    if (cmd == 0xA0) return 3; // Copy CPU->VRAM Header
    if (cmd == 0xC0) return 3; // Copy VRAM->CPU Header

    return 1; // NOP, Config (0xE1-0xE6)
}

u32 Gpu::readStatus() {
    static u64 n = 0;
    n++;
    u32 status = gpu_status | (1 << 26) | (1 << 27) | (1 << 28);

    // Le BIOS attend que le bit 31 bascule (VSync). On alterne à chaque lecture.
    static bool toggle = false;
    toggle = !toggle;
    if (toggle) status |= (1u << 31);
    else        status &= ~(1u << 31);

    if (n % 1000 == 0)   // pas trop de spam
        std::cout << "GPUSTAT lecture #" << std::dec << n
        << " = 0x" << std::hex << status << "\n";

    return status;
}

u32 Gpu::readData() {
    // Si aucun transfert n'est actif, retourne 0
    if (!gp0_transfer) {
        return 0;
    }

    // Récupération du premier pixel 16-bit (bits 0-15 du mot 32-bit)
    u16 pixel1 = vramGet(transfer_x + transfer_cx, transfer_y + transfer_cy);
    transfer_cx++;

    u16 pixel2 = 0;
    // Récupération du second pixel 16-bit (bits 16-31 du mot 32-bit) si la ligne continue
    if (transfer_cx < transfer_w) {
        pixel2 = vramGet(transfer_x + transfer_cx, transfer_y + transfer_cy);
        transfer_cx++;
    }

    // Assemblage des deux pixels 16-bit dans un mot 32-bit
    u32 word = ((u32)pixel2 << 16) | pixel1;
    transfer_words_recus++;

    // Passage à la ligne suivante dans la VRAM si la largeur de la zone est atteinte
    if (transfer_cx >= transfer_w) {
        transfer_cx = 0;
        transfer_cy++;
    }

    // Fin du transfert si tous les mots ou toutes les lignes ont été lus
    if (transfer_words_recus >= transfer_words_total || transfer_cy >= transfer_h) {
        gp0_transfer = false;
    }

    return word;
}

void Gpu::gp0(u32 value) {
    // Mode transfert de pixels CPU -> VRAM

    if (gp0_transfer) {
        for (int k = 0; k < 2; k++) {
            u16 pixel = (value >> (k * 16)) & 0xFFFF;
            vramSet(transfer_x + transfer_cx, transfer_y + transfer_cy, pixel);
            transfer_cx++;
            if (transfer_cx >= transfer_w) {
                transfer_cx = 0;
                transfer_cy++;
                if (transfer_cy >= transfer_h) {
                    gp0_transfer = false;
                    return;              // ? sortie dès que la zone est pleine
                }
            }
        }
        return;
    }

    // Réception d'un nouveau mot de commande
    if (gp0_needed == 0) {
        gp0_command = value >> 24;
        u32 n = calculateGp0Needed(value);
        std::cout << "CMD 0x" << std::hex << gp0_command
            << " attend " << std::dec << n << " mots (mot=0x"
            << std::hex << value << ")\n";
        gp0_command = value >> 24;
        gp0_buffer[0] = value;
        gp0_count = 1;
        gp0_needed = calculateGp0Needed(value);

        if (gp0_needed == 1) {
            u32 cmd = gp0_command; 
            if (cmd == 0xE1) {
                texpage_x = (value & 0xF) * 64;
                texpage_y = ((value >> 4) & 1) * 256;
                tex_depth = (value >> 7) & 3;

                gpu_status = (gpu_status & ~0x7FF) | (value & 0x7FF);
            }
            if (cmd == 0xE5) {
                // Drawing Offset : bits 0-10 = X, bits 11-21 = Y
                draw_offset_x = (s32)((value & 0x7FF) << 21) >> 21;
                draw_offset_y = (s32)(((value >> 11) & 0x7FF) << 21) >> 21;
            }

            gp0_command = 0;
            gp0_needed = 0;
            gp0_count = 0;
            return;
        }

        return;
    }

    // Accumulation des paramètres
    gp0_buffer[gp0_count] = value;
    gp0_count++;

    if (gp0_count < gp0_needed) return;

    // Execution de la commande complète
    u32 cmd = gp0_command;

    if (cmd >= 0x20 && cmd <= 0x3F) {
        bool textured = (cmd >> 2) & 1;
        std::cout << "POLY cmd=0x" << std::hex << cmd
            << (textured ? " TEXTURÉ" : " uni") << "\n";
    }

    if (cmd == 0x02) {
        // Fill Rectangle avec masques hardware PS1
        u32 color = gp0_buffer[0] & 0xFFFFFF;
        u16 x = gp0_buffer[1] & 0x3F0;               // Aligné sur 16 pixels
        u16 y = (gp0_buffer[1] >> 16) & 0x1FF;        // Max 511
        u16 w = ((gp0_buffer[2] & 0x3FF) + 0x0F) & ~0x0F; // Arrondi à 16 pixels
        u16 h = (gp0_buffer[2] >> 16) & 0x1FF;
        fillRectangle(color, x, y, w, h);
    }
    else if (cmd == 0xA0) {
        // Destination dans la VRAM (mot 1)
        transfer_x = gp0_buffer[1] & 0x3FF;
        transfer_y = (gp0_buffer[1] >> 16) & 0x1FF;
        // Taille de la zone (mot 2)
        transfer_w = gp0_buffer[2] & 0x3FF;
        transfer_h = (gp0_buffer[2] >> 16) & 0x1FF;
        if (transfer_w == 0) transfer_w = 0x400;
        if (transfer_h == 0) transfer_h = 0x200;

        if (transfer_w * transfer_h > 0x40000) {
            std::cerr << "A0 taille absurde " << std::dec << transfer_w << "x" << transfer_h
                << ", ignore\n";
            gp0_transfer = false;
        }
        else {
            transfer_cx = 0;
            transfer_cy = 0;
            gp0_transfer = true;
        }
    }
    else if ((cmd & 0xFC) == 0x24) {
        // Decodage de la CLUT (mot 2, bits 16-31)
        u32 clut = (gp0_buffer[2] >> 16) & 0xFFFF;
        clut_x = (clut & 0x3F) * 16;        // bits 0-5  : X en pas de 16
        clut_y = (clut >> 6) & 0x1FF;       // bits 6-14 : Y direct

        // Decodage du TexPage (mot 4, bits 16-31)
        u32 texpage = (gp0_buffer[4] >> 16) & 0xFFFF;
        texpage_x = (texpage & 0xF) * 64;   // bits 0-3 : X en pas de 64
        texpage_y = ((texpage >> 4) & 1) * 256; // bit 4 : Y (0 ou 256)
        tex_depth = (texpage >> 7) & 3;     // bits 7-8 : profondeur

        gpu_status = (gpu_status & ~0x1FF) | (texpage & 0x1FF);

        int x0 = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y0 = parseCoordY(gp0_buffer[1]) + draw_offset_y;
        int x1 = parseCoordX(gp0_buffer[3]) + draw_offset_x;
        int y1 = parseCoordY(gp0_buffer[3]) + draw_offset_y;
        int x2 = parseCoordX(gp0_buffer[5]) + draw_offset_x;
        int y2 = parseCoordY(gp0_buffer[5]) + draw_offset_y;

        drawTriangleTexture(x0, y0, texU(gp0_buffer[2]), texV(gp0_buffer[2]),
            x1, y1, texU(gp0_buffer[4]), texV(gp0_buffer[4]),
            x2, y2, texU(gp0_buffer[6]), texV(gp0_buffer[6]));
    }
    else if ((cmd & 0xFC) == 0x2C) {
        u32 clut = (gp0_buffer[2] >> 16) & 0xFFFF;
        clut_x = (clut & 0x3F) * 16;
        clut_y = (clut >> 6) & 0x1FF;

        u32 texpage = (gp0_buffer[4] >> 16) & 0xFFFF;
        texpage_x = (texpage & 0xF) * 64;
        texpage_y = ((texpage >> 4) & 1) * 256;
        tex_depth = (texpage >> 7) & 3;

        gpu_status = (gpu_status & ~0x1FF) | (texpage & 0x1FF);

        std::cout << "TEXPAGE mot4=0x" << std::hex << gp0_buffer[4]
          << " -> texpage brut=0x" << ((gp0_buffer[4] >> 16) & 0xFFFF) << "\n";

        std::cout << texpage_y << "\n";

        int vx[4], vy[4], vu[4], vv[4];
        for (int i = 0; i < 4; i++) {
            vx[i] = parseCoordX(gp0_buffer[1 + i * 2]) + draw_offset_x;
            vy[i] = parseCoordY(gp0_buffer[1 + i * 2]) + draw_offset_y;
            vu[i] = texU(gp0_buffer[2 + i * 2]);
            vv[i] = texV(gp0_buffer[2 + i * 2]);
        }
        // Un quad = 2 triangles : (0,1,2) et (1,2,3)
        drawTriangleTexture(vx[0], vy[0], vu[0], vv[0],
            vx[1], vy[1], vu[1], vv[1],
            vx[2], vy[2], vu[2], vv[2]);
        drawTriangleTexture(vx[1], vy[1], vu[1], vv[1],
            vx[2], vy[2], vu[2], vv[2],
            vx[3], vy[3], vu[3], vv[3]);
    }
    else if ((cmd & 0xFC) == 0x34) {
        // Triangle Gouraud TEXTURÉ (0x34)
    // Layout :
    // 0: Cmd + Color0          3: Color1             6: Color2
    // 1: X0, Y0                4: X1, Y1             7: X2, Y2
    // 2: U0, V0 + CLUT         5: U1, V1 + TexPage   8: U2, V2

    // 1. Décodage de la CLUT (Mot 2)
        u32 clut = (gp0_buffer[2] >> 16) & 0xFFFF;
        clut_x = (clut & 0x3F) * 16;
        clut_y = (clut >> 6) & 0x1FF;

        // 2. Décodage du TexPage (Mot 5)
        u32 texpage = (gp0_buffer[5] >> 16) & 0xFFFF;
        texpage_x = (texpage & 0xF) * 64;
        texpage_y = ((texpage >> 4) & 1) * 256;
        tex_depth = (texpage >> 7) & 3;

        // Mise à jour de GPUSTAT (ne pas oublier !)
        gpu_status = (gpu_status & ~0x1FF) | (texpage & 0x1FF);

        // 3. Extraction des sommets (+ offset)
        int x0 = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y0 = parseCoordY(gp0_buffer[1]) + draw_offset_y;
        int x1 = parseCoordX(gp0_buffer[4]) + draw_offset_x;
        int y1 = parseCoordY(gp0_buffer[4]) + draw_offset_y;
        int x2 = parseCoordX(gp0_buffer[7]) + draw_offset_x;
        int y2 = parseCoordY(gp0_buffer[7]) + draw_offset_y;

        // 4. Extraction des textures
        int u0 = texU(gp0_buffer[2]); int v0 = texV(gp0_buffer[2]);
        int u1 = texU(gp0_buffer[5]); int v1 = texV(gp0_buffer[5]);
        int u2 = texU(gp0_buffer[8]); int v2 = texV(gp0_buffer[8]);

        // 5. Extraction des couleurs (24-bit brut)
        u32 c0 = gp0_buffer[0] & 0xFFFFFF;
        u32 c1 = gp0_buffer[3] & 0xFFFFFF;
        u32 c2 = gp0_buffer[6] & 0xFFFFFF;

        drawTriangleGouraudTexture(
            x0, y0, u0, v0, c0,
            x1, y1, u1, v1, c1,
            x2, y2, u2, v2, c2
        );
    }
    else if ((cmd & 0xF8) == 0x20) {
        // Triangle monochrome
        u16 c = to555(gp0_buffer[0] & 0xFFFFFF);
        int x0 = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y0 = parseCoordY(gp0_buffer[1]) + draw_offset_y;
        int x1 = parseCoordX(gp0_buffer[2]) + draw_offset_x;
        int y1 = parseCoordY(gp0_buffer[2]) + draw_offset_y;
        int x2 = parseCoordX(gp0_buffer[3]) + draw_offset_x;
        int y2 = parseCoordY(gp0_buffer[3]) + draw_offset_y;
        drawTriangle(x0, y0, x1, y1, x2, y2, c);
    }
    else if ((cmd & 0xF8) == 0x28) {
        // Quad monochrome
        u16 c = to555(gp0_buffer[0] & 0xFFFFFF);
        int vx[4], vy[4];
        for (int i = 0; i < 4; i++) {
            vx[i] = parseCoordX(gp0_buffer[1 + i]) + draw_offset_x;
            vy[i] = parseCoordY(gp0_buffer[1 + i]) + draw_offset_y;
        }
        drawTriangle(vx[0], vy[0], vx[1], vy[1], vx[2], vy[2], c);
        drawTriangle(vx[1], vy[1], vx[2], vy[2], vx[3], vy[3], c);
    }
    else if ((cmd & 0xFC) == 0x30) {
        // Triangle dégradé
        u16 c = to555(gp0_buffer[0] & 0xFFFFFF);
        int x0 = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y0 = parseCoordY(gp0_buffer[1]) + draw_offset_y;
        int x1 = parseCoordX(gp0_buffer[3]) + draw_offset_x;
        int y1 = parseCoordY(gp0_buffer[3]) + draw_offset_y;
        int x2 = parseCoordX(gp0_buffer[5]) + draw_offset_x;
        int y2 = parseCoordY(gp0_buffer[5]) + draw_offset_y;
        drawTriangle(x0, y0, x1, y1, x2, y2, c);
    }
    else if ((cmd & 0xF8) == 0x38) {
        // Quad Gouraud (0x38)
        // Sommets : 0, 1, 2, 3
        // Triangles formés : (0, 1, 2) et (1, 2, 3)

        int x0 = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y0 = parseCoordY(gp0_buffer[1]) + draw_offset_y;
        int x1 = parseCoordX(gp0_buffer[3]) + draw_offset_x;
        int y1 = parseCoordY(gp0_buffer[3]) + draw_offset_y;
        int x2 = parseCoordX(gp0_buffer[5]) + draw_offset_x;
        int y2 = parseCoordY(gp0_buffer[5]) + draw_offset_y;
        int x3 = parseCoordX(gp0_buffer[7]) + draw_offset_x;
        int y3 = parseCoordY(gp0_buffer[7]) + draw_offset_y;

        u32 c0 = gp0_buffer[0] & 0xFFFFFF;
        u32 c1 = gp0_buffer[2] & 0xFFFFFF;
        u32 c2 = gp0_buffer[4] & 0xFFFFFF;
        u32 c3 = gp0_buffer[6] & 0xFFFFFF;

        // Premier triangle (0, 1, 2)
        drawTriangleGouraud(x0, y0, c0, x1, y1, c1, x2, y2, c2);
        // Second triangle (1, 2, 3)
        drawTriangleGouraud(x1, y1, c1, x2, y2, c2, x3, y3, c3);
        }
    else if ((cmd & 0xE0) == 0x60) {
        std::cout << "RECT cmd=0x" << std::hex << cmd
            << " textured=" << ((cmd >> 2) & 1)
            << " size_code=" << std::dec << ((cmd >> 3) & 3) << "\n";
        // ============================================================
        //  RECTANGLES / SPRITES (0x60 - 0x7F)
        //  bits 4-3 : taille  (0=variable, 1=1x1, 2=8x8, 3=16x16)
        //  bit  2   : texture
        //  bit  1   : semi-transparence
        //  bit  0   : raw texture (pas de modulation par la couleur)
        //
        //  Layout selon les drapeaux :
        //    mot 0 : commande + couleur
        //    mot 1 : position (x,y)
        //   [mot 2 : CLUT (bits 16-31) + u,v (bits 0-15)]   si texture
        //   [mot 3 : taille (w,h)]                          si taille variable
        // ============================================================
        bool is_textured = (cmd >> 2) & 1;
        bool is_raw = cmd & 1;
        u32  size_code = (cmd >> 3) & 3;

        int x = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y = parseCoordY(gp0_buffer[1]) + draw_offset_y;

        int u = 0, v = 0;
        u32 idx = 2;   // prochain mot a lire



        if (is_textured) {
            // CLUT dans les bits hauts du mot 2
            u32 clut = (gp0_buffer[2] >> 16) & 0xFFFF;
            clut_x = (clut & 0x3F) * 16;
            clut_y = (clut >> 6) & 0x1FF;
            // Coin haut-gauche de la texture
            u = texU(gp0_buffer[2]);
            v = texV(gp0_buffer[2]);
            idx = 3;
            // NOTE : le TexPage n'est PAS fourni par les rectangles.
            // On garde celui defini par la derniere commande E1.
        }

        int w, h;
        if (size_code == 1) { w = 1;  h = 1; }
        else if (size_code == 2) { w = 8;  h = 8; }
        else if (size_code == 3) { w = 16; h = 16; }
        else {
            // Taille variable : elle est dans le mot suivant
            w = gp0_buffer[idx] & 0x3FF;
            h = (gp0_buffer[idx] >> 16) & 0x1FF;
        }

        if (w == 0 || h == 0) {
            gp0_command = 0; gp0_needed = 0; gp0_count = 0;
            return;
        }

        if (is_textured) {
            u32 couleur = gp0_buffer[0] & 0xFFFFFF;
            drawRectTexture(x, y, w, h, u, v, couleur, is_raw);
        }
        else {
            // Rectangle uni : remplissage direct
            u16 c = to555(gp0_buffer[0] & 0xFFFFFF);
            for (int j = 0; j < h; j++)
                for (int i = 0; i < w; i++)
                    vramSet(x + i, y + j, c);
        }

        if (is_textured) {
            std::cout << "RECT pos=(" << std::dec << x << "," << y << ") "
                << w << "x" << h
                << " uv=(" << u << "," << v << ")"
                << " texpage=(" << texpage_x << "," << texpage_y << ")"
                << " depth=" << (int)tex_depth
                << " clut=(" << clut_x << "," << clut_y << ")"
                << " texel0=0x" << std::hex << lireTexel(u, v) << "\n";
        }
        }
        else if (cmd == 0xC0) {
            // 1. Destination / Origine dans la VRAM (Mot 1)
            transfer_x = gp0_buffer[1] & 0x3FF;
            transfer_y = (gp0_buffer[1] >> 16) & 0x1FF;

            // 2. Dimensions de la zone à lire (Mot 2)
            transfer_w = gp0_buffer[2] & 0x3FF;
            transfer_h = (gp0_buffer[2] >> 16) & 0x1FF;
            if (transfer_w == 0) transfer_w = 0x400;
            if (transfer_h == 0) transfer_h = 0x200;

            // 3. Calcul du nombre total de mots 32-bit (2 pixels 16-bit par mot)
            u32 total_pixels = transfer_w * transfer_h;
            transfer_words_total = (total_pixels + 1) / 2;
            transfer_words_recus = 0;

            // 4. Initialisation des curseurs et activation du transfert
            transfer_cx = 0;
            transfer_cy = 0;
            gp0_transfer = true;
        }
    

    // Réinitialisation complète du state machine
    gp0_command = 0;
    gp0_needed = 0;
    gp0_count = 0;
}

void Gpu::gp1(u32 value) {
    u8 cmd = value >> 24;

    switch (cmd) {
    case 0x00: // Reset GPU
        gpu_status = 0x14000000;
        gp0_count = 0;
        gp0_needed = 0;
        gp0_transfer = false;
        draw_offset_x = 0;
        draw_offset_y = 0;
        break;

    case 0x01: // Reset Command Buffer
        gp0_count = 0;
        gp0_needed = 0;
        gp0_transfer = false;
        break;

    case 0x02: // Acknowledge IRQ
        // Efface le bit 24 (Interrupt Request Flag)
        gpu_status &= ~(1 << 24);
        break;

    case 0x03: // Display Enable
        // Bit 23 de GPUSTAT : 0 = Écran activé, 1 = Écran éteint (Blanking)
        gpu_status = (gpu_status & ~(1 << 23)) | ((value & 1) << 23);
        break;

    case 0x04: // DMA Direction / Request
        // Bits 29-30 de GPUSTAT (0=Off, 1=FIFO, 2=CPU->GPU, 3=GPU->CPU)
        gpu_status = (gpu_status & ~(0x3 << 29)) | ((value & 0x3) << 29);
        break;

    case 0x05: // Display Area Start dans la VRAM
        display_vram_x = value & 0x3FE;              // Aligné sur 2 pixels
        display_vram_y = (value >> 10) & 0x1FF;      // 0 à 511
        break;

    case 0x06: // Horizontal Display Range
        // Définit les limites X1 et X2 à l'écran (utilisé pour les marges TV)
        break;

    case 0x07: // Vertical Display Range
        // Définit les lignes Y1 et Y2 à l'écran (ex: 240 vs 480 lignes)
        break;

    case 0x08: // Display Mode
        // Extrait la résolution, le mode vidéo (PAL/NTSC), la profondeur de couleur (15/24bit)
        // et met à jour les bits 17-22 dans GPUSTAT :
        gpu_status = (gpu_status & ~0x007F0000)
            | ((value & 0x3F) << 17)
            | ((value & 0x40) << 10); // Bit 22 = PAL/NTSC
        break;

    default:
        std::cout << " [GPU GP1] Commande non gérée: " << LOG_HEX(value) << "\n";
        break;
    }
}

u16 Gpu::lireTexel(int u, int v) {
    // Les coordonnees bouclent sur 256 (taille d'une page)
    u &= 0xFF;
    v &= 0xFF;

    if (tex_depth == 2) {
        // --- 15 bits : lecture directe, 1 texel = 1 pixel VRAM ---
        return vramGet(texpage_x + u, texpage_y + v);
    }
    else if (tex_depth == 1) {
        // --- 8 bits : 2 texels par pixel VRAM ---
        // Le pixel VRAM contient 2 index de palette (octet bas / octet haut)
        u16 mot = vramGet(texpage_x + (u / 2), texpage_y + v);
        u8 index = (u & 1) ? (mot >> 8) : (mot & 0xFF);
        // L'index designe une couleur dans la CLUT
        return vramGet(clut_x + index, clut_y);
    }
    else {
        // --- 4 bits : 4 texels par pixel VRAM ---
        u16 mot = vramGet(texpage_x + (u / 4), texpage_y + v);
        // Quel quartet ? (u & 3) donne 0,1,2,3 -> decalage de 0,4,8,12 bits
        u8 index = (mot >> ((u & 3) * 4)) & 0xF;
        return vramGet(clut_x + index, clut_y);
    }
}

void Gpu::fillTestPattern() {
    for (int y = 0; y < VRAM_HEIGHT; y++) {
        for (int x = 0; x < VRAM_WIDTH; x++) {
            u16 r = (x * 31 / VRAM_WIDTH) & 0x1F;
            u16 g = (y * 31 / VRAM_HEIGHT) & 0x1F;
            u16 b = 0;
            vram[y * VRAM_WIDTH + x] = (b << 10) | (g << 5) | r;
        }
    }
}