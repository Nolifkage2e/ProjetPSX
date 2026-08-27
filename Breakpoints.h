#pragma once
#include "common.h"
#include <set>
#include <vector>
#include <string>

// ============================================================
//  Gestion centralisee des points d'arret.
//  - Breakpoints d'execution : declenchent quand le PC les atteint
//  - Watchpoints memoire : declenchent sur lecture ou ecriture
// ============================================================

struct Watchpoint {
    u32  addr;
    bool sur_lecture;
    bool sur_ecriture;
    bool actif = true;
};

class Breakpoints {
    std::set<u32> exec_breaks;          // adresses d'execution
    std::vector<Watchpoint> watches;    // surveillance memoire

    // Rempli quand un point d'arret se declenche
    bool   declenche = false;
    std::string raison;

public:
    // --- Breakpoints d'execution ---
    void ajouterExec(u32 addr) { exec_breaks.insert(addr); }
    void retirerExec(u32 addr) { exec_breaks.erase(addr); }
    bool aExec(u32 addr) const { return exec_breaks.count(addr) > 0; }
    void basculerExec(u32 addr) {
        if (aExec(addr)) retirerExec(addr);
        else             ajouterExec(addr);
    }
    const std::set<u32>& listeExec() const { return exec_breaks; }

    // --- Watchpoints memoire ---
    void ajouterWatch(u32 addr, bool lecture, bool ecriture) {
        watches.push_back({ addr, lecture, ecriture, true });
    }
    void retirerWatch(size_t index) {
        if (index < watches.size()) watches.erase(watches.begin() + index);
    }
    std::vector<Watchpoint>& listeWatch() { return watches; }

    // --- Verification (appelee par le CPU / la Memoire) ---
    bool verifierExec(u32 pc) {
        if (aExec(pc)) {
            declenche = true;
            raison = "Breakpoint execution a 0x" + std::to_string(pc);
            return true;
        }
        return false;
    }

    bool verifierAcces(u32 addr, bool ecriture, u32 valeur, u32 pc) {
        u32 phys = addr & 0x1FFFFFFF;
        for (auto& w : watches) {
            if (!w.actif) continue;
            if ((w.addr & 0x1FFFFFFF) != phys) continue;
            if (ecriture && !w.sur_ecriture) continue;
            if (!ecriture && !w.sur_lecture) continue;

            char buf[128];
            snprintf(buf, sizeof(buf),
                "Watchpoint %s a 0x%08X (valeur 0x%08X) depuis PC=0x%08X",
                ecriture ? "ECRITURE" : "LECTURE", addr, valeur, pc);
            raison = buf;
            declenche = true;
            return true;
        }
        return false;
    }

    // --- Etat ---
    bool estDeclenche() const { return declenche; }
    const std::string& getRaison() const { return raison; }
    void reinitialiser() { declenche = false; raison.clear(); }
};
