#include "Ecran.h"
#include "Gpu.h"     // pour VRAM_WIDTH / VRAM_HEIGHT
#include <iostream>

Ecran::Ecran(int w, int h) : width(w), height(h) {}

Ecran::~Ecran() {
    if (texture)  SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();
}

bool Ecran::init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init erreur : " << SDL_GetError() << "\n";
        return false;
    }

    window = SDL_CreateWindow("ProjetPSX - VRAM",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "CreateWindow erreur : " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "CreateRenderer erreur : " << SDL_GetError() << "\n";
        return false;
    }

    // Texture streaming : on la met a jour chaque frame avec la VRAM.
    // Format RGBA8888 : on convertira le BGR555 de la VRAM vers ce format.
    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        VRAM_WIDTH, VRAM_HEIGHT);
    if (!texture) {
        std::cerr << "CreateTexture erreur : " << SDL_GetError() << "\n";
        return false;
    }

    return true;
}

void Ecran::afficherVram(const u16* vram) {
    // On verrouille la texture pour ecrire dedans directement.
    void* pixels;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0) {
        std::cerr << "LockTexture erreur : " << SDL_GetError() << "\n";
        return;
    }

    u32* dst = (u32*)pixels;
    for (int i = 0; i < VRAM_WIDTH * VRAM_HEIGHT; i++) {
        u16 c = vram[i];
        // BGR555 -> composantes 5 bits
        u8 r = (c & 0x1F);
        u8 g = ((c >> 5) & 0x1F);
        u8 b = ((c >> 10) & 0x1F);
        // Etendre 5 bits -> 8 bits (x8 approximatif : <<3)
        u8 r8 = (r << 3) | (r >> 2);
        u8 g8 = (g << 3) | (g >> 2);
        u8 b8 = (b << 3) | (b >> 2);
        // RGBA8888 : R,G,B,A
        dst[i] = (r8 << 24) | (g8 << 16) | (b8 << 8) | 0xFF;
    }

    SDL_UnlockTexture(texture);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

bool Ecran::gererEvenements() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) return false;
    }
    return true;
}
