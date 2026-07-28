#include "Gpu.h"
#include <iostream>
#include <iomanip>

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
    u32 status = gpu_status;

    // Masques pour indiquer au CPU/DMA que le GPU est prêt
    status |= (1 << 26); // Prêt à recevoir une commande GP0
    status |= (1 << 27); // Prêt à envoyer des données VRAM
    status |= (1 << 28); // Prêt à recevoir un bloc DMA

    // --- SIMULATION DU MATÉRIEL ---
    // Fait basculer le bit 31 (Odd/Even scanline ou GPU Busy) 
    // et le bit 19 (Vertical Interlace) à chaque lecture.
    // Cela trompe le BIOS en lui faisant croire que le faisceau 
    // vidéo balaie l'écran et que le temps passe.
    static bool hardware_toggle = false;
    hardware_toggle = !hardware_toggle;

    u32 dma_dir = (gpu_status >> 29) & 3;

    if (dma_dir == 1) {
        status |= (1 << 25); // FIFO : Toujours prêt (1)
    }
    else if (dma_dir == 2) {
        status |= (1 << 25); // CPU vers GPU : Miroir du bit 28
    }
    else if (dma_dir == 3) {
        status |= (1 << 25); // GPU vers CPU : Miroir du bit 27
    }
    else {
        status &= ~(1 << 25); // Off (0)
    }

    static u64 global_read_ticks = 0;
    global_read_ticks++;

    if ((global_read_ticks / 10000) % 2 == 0) {
        status |= (1 << 31);
        status |= (1 << 19); // Ligne impaire
    }
    else {
        status &= ~(1 << 31);
        status &= ~(1 << 19); // Ligne paire
    }
    return status;
}

u32 Gpu::readData() {
    return 0;
}

void Gpu::gp0(u32 value) {
    // Mode transfert de pixels CPU -> VRAM
    if (gp0_transfer) {
        u16 p0 = value & 0xFFFF;
        u16 p1 = (value >> 16) & 0xFFFF;

        // Pixel 0
        if (transfer_cy < transfer_h) {
            vramSet(transfer_x + transfer_cx, transfer_y + transfer_cy, p0);
            transfer_cx++;
            if (transfer_cx >= transfer_w) {
                transfer_cx = 0;
                transfer_cy++;
            }
        }

        // Pixel 1
        if (transfer_cy < transfer_h) {
            vramSet(transfer_x + transfer_cx, transfer_y + transfer_cy, p1);
            transfer_cx++;
            if (transfer_cx >= transfer_w) {
                transfer_cx = 0;
                transfer_cy++;
            }
        }

        if (transfer_cy >= transfer_h) {
            gp0_transfer = false;
        }
        return;
    }

    // Réception d'un nouveau mot de commande
    if (gp0_needed == 0) {
        gp0_command = value >> 24;
        gp0_buffer[0] = value;
        gp0_count = 1;
        gp0_needed = calculateGp0Needed(value);

        if (gp0_needed == 1) {
            u32 cmd = gp0_command;
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

    if (cmd == 0x02) {
        // Fill Rectangle avec masques hardware PS1
        u32 color = gp0_buffer[0] & 0xFFFFFF;
        u16 x = gp0_buffer[1] & 0x3F0;               // Aligné sur 16 pixels
        u16 y = (gp0_buffer[1] >> 16) & 0x1FF;        // Max 511
        u16 w = ((gp0_buffer[2] & 0x3FF) + 0x0F) & ~0x0F; // Arrondi à 16 pixels
        u16 h = (gp0_buffer[2] >> 16) & 0x1FF;
        fillRectangle(color, x, y, w, h);
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
    else if ((cmd & 0xF8) == 0x30) {
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
        // Quad dégradé
        u16 c = to555(gp0_buffer[0] & 0xFFFFFF);
        int x0 = parseCoordX(gp0_buffer[1]) + draw_offset_x;
        int y0 = parseCoordY(gp0_buffer[1]) + draw_offset_y;
        int x1 = parseCoordX(gp0_buffer[3]) + draw_offset_x;
        int y1 = parseCoordY(gp0_buffer[3]) + draw_offset_y;
        int x2 = parseCoordX(gp0_buffer[5]) + draw_offset_x;
        int y2 = parseCoordY(gp0_buffer[5]) + draw_offset_y;
        int x3 = parseCoordX(gp0_buffer[7]) + draw_offset_x;
        int y3 = parseCoordY(gp0_buffer[7]) + draw_offset_y;
        drawTriangle(x0, y0, x1, y1, x2, y2, c);
        drawTriangle(x1, y1, x2, y2, x3, y3, c);
    }
    else if (cmd == 0xA0) {
        transfer_x = gp0_buffer[1] & 0x3FE;
        transfer_y = (gp0_buffer[1] >> 16) & 0x1FF;
        transfer_w = gp0_buffer[2] & 0x3FF;
        transfer_h = (gp0_buffer[2] >> 16) & 0x1FF;
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