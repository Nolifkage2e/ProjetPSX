#pragma once
#include "common.h"
#include <string>

// Traduit une instruction MIPS R3000A en texte lisible.
// addr sert a calculer les cibles de branchement absolues.
std::string desassembler(u32 instr, u32 addr);

// Nom ABI d'un registre (0 = "zero", 4 = "a0", 31 = "ra"...)
const char* nomRegistre(u32 index);