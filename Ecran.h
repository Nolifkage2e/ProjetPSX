#pragma once

#include "common.h"
#include <SDL.h>

// Gere la fenetre SDL et l'affichage de la VRAM.
// Separe du GPU : le GPU produit des pixels, Ecran les montre.
class Ecran {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;

    int width;
    int height;

public:
    Ecran(int w = 1024, int h = 512);
    ~Ecran();

    bool init();

    // Convertit la VRAM (BGR555) en RGBA8888 et l'affiche.
    void afficherVram(const u16* vram);

    // Traite les evenements (fermeture...). Retourne false si on doit quitter.
    bool gererEvenements();


};

