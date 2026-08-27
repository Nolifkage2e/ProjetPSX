#pragma once

#include "common.h"
#include <SDL.h>
#include <vector>
#include <set>

class Emulateur;
class Memoire;
class Gpu;
class CPU;

// ============================================================
//  Debogueur visuel base sur Dear ImGui.
//  Fenetres : vue memoire (hex viewer), vue VRAM, controles.
// ============================================================
class Debogueur {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    // Texture pour afficher la VRAM dans une fenetre ImGui
    SDL_Texture* vramTexture = nullptr;

    Emulateur& emu;

    // --- Etat de la vue memoire ---
    u32  memoire_addr = 0x80000000;   // adresse affichee
    char addr_input[16] = "80000000"; // champ de saisie
    int  memoire_zone = 0;            // 0=RAM, 1=BIOS, 2=Registres IO

    // --- Etat de la vue VRAM ---
    float vram_zoom = 1.0f;
    bool  vram_afficher_grille = true;
    int   vram_survol_x = 0, vram_survol_y = 0;

    // --- Controles d'execution ---
    bool en_pause = false;
    bool step_demande = false;
    u64  instructions_par_frame = 564480;   // ~1 frame de CPU

    void fenetreDesassembleur();
    void fenetreRegistres();
    void fenetreBreakpoints();
    u32  desasm_addr = 0xBFC00000;
    bool suivre_pc = true;
    char watch_input[16] = "80077FA0";
    bool watch_lecture = false;
    bool watch_ecriture = true;

public:
    Debogueur(Emulateur& e) : emu(e) {}
    ~Debogueur();

    bool init();
    void nouvelleFrame();          // debut de frame ImGui
    void dessiner();               // dessine toutes les fenetres
    void presenter();              // fin de frame + affichage
    bool gererEvenements();  
    void mettreEnPause() { en_pause = true; }// retourne false si on doit quitter

    // Etat pour la boucle principale
    bool estEnPause() const { return en_pause; }
    bool consommerStep() { bool s = step_demande; step_demande = false; return s; }
    u64  getInstructionsParFrame() const { return instructions_par_frame; }

private:
    void fenetreControles();
    void fenetreMemoire();
    void fenetreVram();
    void majTextureVram();
};

