#include "Debogueur.h"
#include "Emulateur.h"
#include "Memoire.h"
#include "Gpu.h"
#include "Cpu.h"
#include "Desassembleur.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <cstdio>
#include <cstring>

Debogueur::~Debogueur() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (vramTexture) SDL_DestroyTexture(vramTexture);
    if (renderer)    SDL_DestroyRenderer(renderer);
    if (window)      SDL_DestroyWindow(window);
    SDL_Quit();
}

bool Debogueur::init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

    window = SDL_CreateWindow("ProjetPSX - Debogueur",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1600, 900, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) return false;

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) return false;

    // Texture pour la VRAM (1024x512 en RGBA)
    vramTexture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        VRAM_WIDTH, VRAM_HEIGHT);

    // --- Initialisation ImGui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    return true;
}

bool Debogueur::gererEvenements() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT) return false;
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: return false;
            case SDLK_F5:     en_pause = !en_pause; break;   // pause/continuer
            case SDLK_F10:    step_demande = true; break;    // step
            }
        }
    }
    return true;
}

void Debogueur::nouvelleFrame() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Debogueur::dessiner() {
    fenetreControles();
    fenetreMemoire();
    fenetreVram();
    fenetreDesassembleur();
    fenetreRegistres();
    fenetreBreakpoints();
}

void Debogueur::presenter() {
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}

// ============================================================
//  Fenetre : controles d'execution
// ============================================================
void Debogueur::fenetreControles() {
    ImGui::Begin("Controles");

    if (ImGui::Button(en_pause ? "Continuer (F5)" : "Pause (F5)")) {
        en_pause = !en_pause;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step (F10)")) {
        step_demande = true;
        en_pause = true;
    }

    ImGui::Separator();

    int ipf = (int)instructions_par_frame;
    if (ImGui::SliderInt("Instructions/frame", &ipf, 1000, 2000000)) {
        instructions_par_frame = (u64)ipf;
    }

    ImGui::Separator();
    ImGui::Text("PC : 0x%08X", emu.getCpu().getPc());
    ImGui::Text("Etat : %s", en_pause ? "EN PAUSE" : "En cours");

    ImGui::End();
}

// ============================================================
//  Fenetre : vue memoire (hex viewer)
// ============================================================
void Debogueur::fenetreMemoire() {
    ImGui::Begin("Memoire");

    // Boutons de saut rapide
    if (ImGui::Button("RAM")) { memoire_addr = 0x80000000; }
    ImGui::SameLine();
    if (ImGui::Button("BIOS")) { memoire_addr = 0xBFC00000; }
    ImGui::SameLine();
    if (ImGui::Button("Registres")) { memoire_addr = 0x1F801000; }
    ImGui::SameLine();
    if (ImGui::Button("-> PC")) { memoire_addr = emu.getCpu().getPc() & ~0xF; }

    // Champ de saisie d'adresse
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText("Adresse", addr_input, sizeof(addr_input),
        ImGuiInputTextFlags_CharsHexadecimal |
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        u32 a = 0;
        sscanf_s(addr_input, "%x", &a);
        memoire_addr = a & ~0xF;   // aligne sur 16
    }
    ImGui::SameLine();
    if (ImGui::Button("<<")) memoire_addr -= 0x100;
    ImGui::SameLine();
    if (ImGui::Button(">>")) memoire_addr += 0x100;

    ImGui::Separator();

    // Le hex viewer : 16 octets par ligne, 32 lignes
    ImGui::BeginChild("hex", ImVec2(0, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    Memoire& mem = emu.getMemoire();

    for (int ligne = 0; ligne < 32; ligne++) {
        u32 base = memoire_addr + ligne * 16;

        // Colonne adresse
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "%08X", base);
        ImGui::SameLine();

        // Colonne hexa
        char hexbuf[64] = {};
        char ascii[17] = {};
        int  pos = 0;
        for (int i = 0; i < 16; i++) {
            u8 octet = mem.load8(base + i);
            pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X ", octet);
            ascii[i] = (octet >= 32 && octet < 127) ? (char)octet : '.';
        }
        ImGui::Text("%s", hexbuf);
        ImGui::SameLine();

        // Colonne ASCII
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "|%s|", ascii);
    }

    ImGui::EndChild();
    ImGui::End();
}

// ============================================================
//  Fenetre : vue VRAM
// ============================================================
void Debogueur::majTextureVram() {
    void* pixels; int pitch;
    if (SDL_LockTexture(vramTexture, nullptr, &pixels, &pitch) != 0) return;

    const u16* vram = emu.getGpu().getVram();
    u32* dst = (u32*)pixels;

    for (int i = 0; i < VRAM_WIDTH * VRAM_HEIGHT; i++) {
        u16 c = vram[i];
        u8 r = (c & 0x1F);
        u8 g = ((c >> 5) & 0x1F);
        u8 b = ((c >> 10) & 0x1F);
        u8 r8 = (r << 3) | (r >> 2);
        u8 g8 = (g << 3) | (g >> 2);
        u8 b8 = (b << 3) | (b >> 2);
        dst[i] = (r8 << 24) | (g8 << 16) | (b8 << 8) | 0xFF;
    }
    SDL_UnlockTexture(vramTexture);
}

void Debogueur::fenetreVram() {
    ImGui::Begin("VRAM");

    majTextureVram();

    ImGui::SliderFloat("Zoom", &vram_zoom, 0.25f, 3.0f);
    ImGui::SameLine();
    ImGui::Checkbox("Grille pages", &vram_afficher_grille);

    ImGui::Separator();

    ImVec2 taille(VRAM_WIDTH * vram_zoom, VRAM_HEIGHT * vram_zoom);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::Image((ImTextureID)(intptr_t)vramTexture, taille);

    // Grille des pages de texture (256x256)
    if (vram_afficher_grille) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImU32 couleur = IM_COL32(255, 255, 0, 80);
        for (int x = 0; x <= VRAM_WIDTH; x += 64) {
            float fx = pos.x + x * vram_zoom;
            draw->AddLine(ImVec2(fx, pos.y),
                ImVec2(fx, pos.y + taille.y), couleur);
        }
        for (int y = 0; y <= VRAM_HEIGHT; y += 256) {
            float fy = pos.y + y * vram_zoom;
            draw->AddLine(ImVec2(pos.x, fy),
                ImVec2(pos.x + taille.x, fy), couleur);
        }
    }

    // Coordonnees + couleur sous le curseur
    if (ImGui::IsItemHovered()) {
        ImVec2 souris = ImGui::GetMousePos();
        vram_survol_x = (int)((souris.x - pos.x) / vram_zoom);
        vram_survol_y = (int)((souris.y - pos.y) / vram_zoom);
        if (vram_survol_x >= 0 && vram_survol_x < VRAM_WIDTH &&
            vram_survol_y >= 0 && vram_survol_y < VRAM_HEIGHT) {
            u16 c = emu.getGpu().getVram()[vram_survol_y * VRAM_WIDTH + vram_survol_x];
            ImGui::BeginTooltip();
            ImGui::Text("(%d, %d)", vram_survol_x, vram_survol_y);
            ImGui::Text("0x%04X", c);
            ImGui::Text("R=%d G=%d B=%d",
                c & 0x1F, (c >> 5) & 0x1F, (c >> 10) & 0x1F);
            ImGui::EndTooltip();
        }
    }

    ImGui::End();
}

void Debogueur::fenetreDesassembleur() {
    ImGui::Begin("Desassembleur");

    u32 pc = emu.getCpu().getPc();

    ImGui::Checkbox("Suivre le PC", &suivre_pc);
    ImGui::SameLine();
    if (ImGui::Button("-> PC")) { desasm_addr = pc; }
    ImGui::SameLine();
    if (ImGui::Button("<<")) desasm_addr -= 0x40;
    ImGui::SameLine();
    if (ImGui::Button(">>")) desasm_addr += 0x40;

    if (suivre_pc) {
        // Centre la vue sur le PC (16 instructions avant)
        desasm_addr = pc - 0x40;
    }

    ImGui::Separator();
    ImGui::TextDisabled("Clic sur une ligne = poser/retirer un breakpoint");

    ImGui::BeginChild("code", ImVec2(0, 0), true);

    Memoire& mem = emu.getMemoire();
    Breakpoints& bp = emu.getBreakpoints();

    for (int i = 0; i < 40; i++) {
        u32 addr = desasm_addr + i * 4;
        u32 instr = mem.load32(addr);

        bool est_pc = (addr == pc);
        bool a_break = bp.aExec(addr);

        // Marqueur de breakpoint
        if (a_break) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "*");
        }
        else {
            ImGui::TextDisabled(" ");
        }
        ImGui::SameLine();

        // Ligne : adresse + mot + desassemblage
        char ligne[160];
        snprintf(ligne, sizeof(ligne), "%08X  %08X  %s",
            addr, instr, desassembler(instr, addr).c_str());

        ImVec4 couleur = est_pc
            ? ImVec4(1.0f, 1.0f, 0.4f, 1.0f)    // instruction courante : jaune
            : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, couleur);
        if (ImGui::Selectable(ligne, est_pc)) {
            bp.basculerExec(addr);   // clic = poser/retirer un breakpoint
        }
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::End();
}

void Debogueur::fenetreRegistres() {
    ImGui::Begin("Registres");

    CPU& cpu = emu.getCpu();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f),
        "PC   = 0x%08X", cpu.getPc());
    ImGui::Text("HI   = 0x%08X", cpu.getHi());
    ImGui::Text("LO   = 0x%08X", cpu.getLo());

    ImGui::Separator();

    // Les 32 registres en 2 colonnes
    if (ImGui::BeginTable("regs", 4, ImGuiTableFlags_Borders)) {
        for (int i = 0; i < 16; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f),
                "%-4s", nomRegistre(i));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%08X", cpu.getReg(i));

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f),
                "%-4s", nomRegistre(i + 16));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%08X", cpu.getReg(i + 16));
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("COP0");
    ImGui::Text("SR    = 0x%08X", cpu.getCop0(12));
    ImGui::Text("CAUSE = 0x%08X", cpu.getCop0(13));
    ImGui::Text("EPC   = 0x%08X", cpu.getCop0(14));

    ImGui::End();
}

void Debogueur::fenetreBreakpoints() {
    ImGui::Begin("Points d'arret");

    Breakpoints& bp = emu.getBreakpoints();

    // --- Message si un point d'arret vient de se declencher ---
    if (bp.estDeclenche()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
            "%s", bp.getRaison().c_str());
        if (ImGui::Button("Acquitter")) {
            bp.reinitialiser();
        }
        ImGui::Separator();
    }

    // --- Ajout d'un watchpoint ---
    ImGui::Text("Nouveau watchpoint memoire :");
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Adresse##watch", watch_input, sizeof(watch_input),
        ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::Checkbox("Lecture", &watch_lecture);
    ImGui::SameLine();
    ImGui::Checkbox("Ecriture", &watch_ecriture);
    ImGui::SameLine();
    if (ImGui::Button("Ajouter")) {
        u32 a = 0;
        sscanf_s(watch_input, "%x", &a);
        bp.ajouterWatch(a, watch_lecture, watch_ecriture);
    }

    ImGui::Separator();

    // --- Liste des watchpoints ---
    ImGui::Text("Watchpoints actifs :");
    auto& watches = bp.listeWatch();
    for (size_t i = 0; i < watches.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::Checkbox("##actif", &watches[i].actif);
        ImGui::SameLine();
        ImGui::Text("0x%08X  %s%s", watches[i].addr,
            watches[i].sur_lecture ? "R" : "",
            watches[i].sur_ecriture ? "W" : "");
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            bp.retirerWatch(i);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::Separator();

    // --- Liste des breakpoints d'execution ---
    ImGui::Text("Breakpoints execution :");
    for (u32 addr : bp.listeExec()) {
        ImGui::Text("0x%08X", addr);
        ImGui::SameLine();
        ImGui::PushID((int)addr);
        if (ImGui::SmallButton("X")) {
            bp.retirerExec(addr);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::End();
}


