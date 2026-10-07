# ProjetPSX

> Un émulateur PlayStation 1 écrit de 0 en C++, sans dépendance
> d'émulation externe — CPU, mémoire, DMA, GPU et affichage entièrement
> implémentés à la main à partir de la documentation matérielle et de l'IA.

<img width="907" height="540" alt="image" src="https://github.com/user-attachments/assets/465a7f79-4b37-40c2-b8b0-fe6f7a512622" />
*Le logo de démarrage PlayStation, rendu par le rasterizer maison (gouraud shading)*

---

## Sommaire

- [Aperçu](#aperçu)
- [État du projet](#état-du-projet)
- [Captures d'écran](#captures-décran)
- [Architecture](#architecture)
- [Défis techniques](#défis-techniques)
- [Compilation](#compilation)
- [Utilisation](#utilisation)
- [Feuille de route](#feuille-de-route)
- [Ressources](#ressources)

---
## Introduction
Ce projet est réalisé dans un but éducatif et en étroite collaboration avec l'IA conversationnelle Claude (Anthropic).
Il a pour but principal d'approfondir mes connaissances sur l'ingénierie matérielle, sur l'architecture des processeurs, sur la de
gestion mémoire bas niveau, sur les pipelines graphiques, etc. en reconstruisant une console de A à Z.

L'objectif est double. Au-delà du contenu technique, ce projet est aussi un exercice délibéré de collaboration avec l'IA comme outil d'ingénierie.
L'industrie évolue rapidement vers des flux de travail où le développeur ne code plus seul : il dirige, questionne, valide et intègre. 
Apprendre à travailler efficacement dans ce mode, savoir poser les bonnes questions, reconnaître quand une piste proposée est fausse et
exiger la compréhension avant l'implémentation me semble être une compétence aussi déterminante que la maîtrise du langage lui-même.

L'IA a servi de mentor et d'accélérateur, pas de substitut au raisonnement. 
Plusieurs bugs ont d'ailleurs été trouvés en remettant en question des diagnostics erronés, 
ou en désassemblant le BIOS à la main pour confronter les hypothèses à la réalité du matériel.

Le résultat est un projet d'envergure et
la démonstration d'un usage rigoureux des outils de demain.

*Dave-Hardens Odigé*  
*Étudiant à l'Université Laval au Baccalauréat en génie informatique*

---
## Aperçu

ProjetPSX émule le matériel de la Sony PlayStation (1994) : le processeur
MIPS R3000A, son coprocesseur système, le bus mémoire, le contrôleur DMA
et le GPU. L'émulateur démarre le BIOS d'origine, exécute son noyau et
affiche l'animation de démarrage.

Le projet a été construit **subsystème par subsystème**, en partant d'un
interpréteur d'instructions minimal jusqu'au rendu graphique complet, en
s'appuyant uniquement sur la documentation matérielle (psx-spx) et le
débogage par désassemblage du BIOS.

**Stack technique** : C++14, SDL2, Visual Studio 2022.

---

## État du projet

### Implémenté

| Sous-système | Détail |
|---|---|
| **CPU MIPS R3000A** | Jeu d'instructions complet (~60 opcodes), branch delay slots, load delay slots, registres HI/LO, comportement matériel de la division par zéro |
| **Mémoire** | RAM 2 Mo, BIOS 512 Ko, masquage des segments KUSEG/KSEG0/KSEG1/KSEG2, accès 8/16/32 bits |
| **COP0** | Registres SR/CAUSE/EPC, MFC0/MTC0/RFE, mécanisme complet d'exceptions avec pile de mode, SYSCALL, BREAK, gestion du delay slot (bit BD) |
| **Interruptions** | I_STAT / I_MASK avec sémantique d'acquittement par masque, VBlank |
| **DMA** | 7 canaux, registres MADR/BCR/CHCR, DPCR/DICR, SyncMode 0 (bloc), SyncMode 1 (blocs synchronisés), SyncMode 2 (linked list), canal OTC |
| **GPU** | VRAM 1024×512 en BGR555, machine à états GP0 multi-mots, commandes GP1, Fill Rectangle, rasterizer de triangles (edge functions), gouraud shading, texture mapping (4/8/15 bits + CLUT), transferts CPU→VRAM |
| **Affichage** | SDL2, texture streaming, conversion BGR555 → RGBA8888 |

### En cours

- Rendu des sprites texturés (police du BIOS)
- Synchronisation fine des transferts DMA en mode bloc

### À venir

- Sous-système CD-ROM
- GTE (Geometry Transformation Engine) pour la 3D
- SPU (audio)
- Manettes

---

## Captures d'écran

> Remplacer les images ci-dessous par vos propres captures dans `screenshots/`.

### Boot du BIOS

![Boot](screenshots/boot.png)
*Le BIOS initialise le matériel : memory control, cache, SPU, timers*

### Logo de démarrage

![Logo](screenshots/logo.png)
*Rendu du logo PlayStation — polygones gouraud interpolés*

### Vue complète de la VRAM

![VRAM](screenshots/vram.png)
*Vue de débogage : l'intégralité de la VRAM 1024×512, avec les pages de
texture visibles hors zone d'affichage*

### Rasterizer — motif de test

![Test pattern](screenshots/testpattern.png)
*Validation de la chaîne VRAM → écran : dégradé RGB sur 15 bits*

---

## Architecture

```
main.cpp          Point d'entrée, boucle principale (émulation + affichage)
Emulateur.*       Chef d'orchestre : possède et relie tous les sous-systèmes
Cpu.*             CPU MIPS R3000A + COP0 (exceptions, interruptions)
Memoire.*         Bus mémoire, masquage des segments, décodage des périphériques
Dma.*             Contrôleur DMA 7 canaux
Gpu.*             VRAM, machine à états GP0/GP1, rasterizer
Interruptions.h   Contrôleur d'interruptions (I_STAT / I_MASK)
Ecran.*           Fenêtre SDL2, conversion et affichage de la VRAM
common.h          Types (u8/u16/u32, s8/s16/s32) et macros partagées
```

### Flux d'une commande de dessin

```
CPU (sw vers 0x1F801810)
   ↓
Memoire::store32 — décodage d'adresse
   ↓
Gpu::gp0 — machine à états (accumulation des paramètres)
   ↓
Rasterizer — écriture des pixels dans la VRAM
   ↓
Ecran::afficherVram — conversion BGR555 → RGBA8888 → SDL
```

---

## Défis techniques

Quelques problèmes marquants rencontrés et résolus, chacun ayant demandé
un diagnostic par instrumentation et désassemblage du code invité.

**Load delay slots manquants.** Sur MIPS, la valeur chargée par un `lw`
n'est pas disponible pour l'instruction suivante. Leur absence corrompait
silencieusement les séquences *read-modify-write* du BIOS : une valeur de
registre DMA fuyait dans un autre registre, déclenchant un faux transfert
qui écrasait la pile. Diagnostic établi en désassemblant à la main la
routine fautive et en traçant les écritures parasites.

**Exceptions dans un delay slot.** Lorsqu'une interruption survient pendant
l'exécution d'un delay slot, EPC doit pointer sur le branchement et non sur
le delay slot. Sans cette correction, le retour d'exception atterrissait une
instruction trop loin et le noyau tombait sur ses propres gardes-fous
(`SystemErrorUnresolvedException`).

**Cache isolation au boot.** Le BIOS active le bit IsC du registre SR pour
« flusher » le cache en écrivant partout ; ces écritures doivent être
ignorées, faute de quoi le BIOS écrase sa propre mémoire.

**Débordement de pile à l'initialisation.** Les 2,5 Mo de RAM + BIOS stockés
en `std::array` membres dépassaient la pile Windows de 1 Mo. Migration vers
`std::vector` (allocation sur le tas).

**Désynchronisation de la machine à états GP0.** Les commandes GPU ayant un
nombre de mots variable, une seule erreur de comptage décale tout le flux
suivant : les données pixel sont alors interprétées comme des commandes.
Diagnostiqué en instrumentant chaque transition d'état.

---

## Compilation

### Prérequis

- Visual Studio 2022 (ou tout compilateur C++17)
- SDL2 (version 2.x — **pas** SDL3)
- Un fichier BIOS PlayStation (non fourni, voir ci-dessous)

### Configuration SDL2

1. Télécharger `SDL2-devel-2.x.x-VC.zip` depuis
   [les releases SDL](https://github.com/libsdl-org/SDL/releases)
2. Extraire dans un dossier stable (ex. `C:\vclib\SDL2-2.x.x`)
3. Dans les propriétés du projet (toutes configurations, x64) :
   - **C/C++ → Général → Autres répertoires Include** : `<chemin>\include`
   - **Éditeur de liens → Général → Répertoires de bibliothèques** : `<chemin>\lib\x64`
   - **Éditeur de liens → Entrée → Dépendances** : `SDL2.lib;SDL2main.lib`
   - **Éditeur de liens → Système → Sous-système** : `Console`
4. Copier `SDL2.dll` à côté de l'exécutable généré

### BIOS

Le fichier BIOS n'est pas inclus dans ce dépôt (code propriétaire Sony).
Placer un dump de BIOS PlayStation dans `bios/` et ajuster le chemin dans
`main.cpp`. Le dossier `bios/` est exclu via `.gitignore`.

---

## Utilisation

```
ProjetPSX.exe
```

L'émulateur charge le BIOS, ouvre une fenêtre affichant la VRAM et exécute
le démarrage. Fermer la fenêtre ou appuyer sur `Échap` pour quitter.

### Options de débogage

Plusieurs canaux de journalisation sont disponibles via des drapeaux dans
le code (`LOG_GPU`, `LOG_DMA`, `LOG_IRQ`, `LOG_SPU`). Ils sont désactivés
par défaut : les activer ralentit considérablement l'exécution mais permet
de tracer précisément le comportement de chaque sous-système.

---

## Feuille de route

- [x] CPU MIPS R3000A complet
- [x] Bus mémoire et segments
- [x] Exceptions et interruptions (COP0)
- [x] DMA (3 modes de synchronisation)
- [x] GPU : rasterizer et texture mapping
- [x] Affichage SDL2
- [ ] Sprites texturés (police du BIOS)
- [ ] CD-ROM (chargement de jeux)
- [ ] GTE (transformations 3D)
- [ ] SPU (audio)
- [ ] Manettes

---

## Ressources

- **[psx-spx](https://psx-spx.consoledev.net/)** — la documentation de
  référence du matériel PlayStation (Martin Korth)
- **[Guide de Lionel Flandrin](https://github.com/simias/psx-guide)** —
  construction d'un émulateur PS1 pas à pas
- **Amidog CPU tests** — suite de tests de conformité du CPU
- **Claude AI - Anthropic

---

## Licence

Projet personnel à but éducatif. Le BIOS PlayStation et les jeux sont la
propriété de Sony Interactive Entertainment et ne sont pas distribués ici.
