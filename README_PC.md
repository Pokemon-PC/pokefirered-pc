# Pokémon Rouge Feu / Vert Feuille — version PC native

Exécutables PC compilés depuis la décompilation pret/pokefirered, sans émulateur :
`pokefirered` (Rouge Feu) et `pokeleafgreen` (Vert Feuille). La ROM GBA se construit
toujours avec `make` comme avant (sources inchangées : tout le code PC est dans
`src/platform_pc/` ou sous `#ifdef PLATFORM_PC`).

Pas besoin de ROM : toutes les données du jeu (version anglaise US) sont dans ce dépôt.

## Prérequis (Windows)

- **Clang** (llvm-mingw), **CMake** ≥ 3.20, **Ninja**
- **GNU binutils + make** (WinLibs : fournit `as` et `mingw32-make`) — l'assembleur GNU
  est nécessaire pour les données du jeu, celui de LLVM ne suffit pas
- **Python 3**

SDL2, zlib et libpng sont téléchargés et compilés automatiquement par CMake.

Antivirus : certains (AVG, Avast…) retiennent les exécutables qui viennent d'être
compilés (ils restent bloqués à 0 % de CPU). Ajoutez une exception sur le dossier du dépôt.

## Prérequis (Linux)

```
sudo apt install clang cmake ninja-build python3 binutils make zlib1g-dev libasound2-dev libpulse-dev libx11-dev libxext-dev libgl-dev
```

Mêmes commandes de construction (avec `python3`) ; l'exe s'appelle `build-pc/pokefirered`.

## Construire

```
cmake -S src/platform_pc/tools -B build-tools -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build-tools
python src/platform_pc/scripts/gen_assets.py -j 16
cmake -B build-pc -G Ninja -DCMAKE_C_COMPILER=clang
cmake --build build-pc
cmake -B build-lg -G Ninja -DCMAKE_C_COMPILER=clang -DPFR_GAME_VERSION=LEAFGREEN
cmake --build build-lg
```

1. les outils de la décomp (`gbagfx`, `preproc`, `mapjson`…) ;
2. les données (graphismes, cartes, musique, communes aux deux versions) ;
3. Rouge Feu : `build-pc/pokefirered.exe` ; Vert Feuille : `build-lg/pokeleafgreen.exe`.

Tests : `cd build-pc && ctest` (couche matériel ; démarrage, intro, titre, nouvelle
partie avec la musique ; maison, Bourg Palette et un combat ; menus, sauvegarde et reprise).

Paquet à distribuer : `python src/platform_pc/scripts/package.py` →
`dist/pokefirered-pc-<commit>.zip` (les exe construits + README ; jamais de sauvegarde,
de ROM ni de config).

## Jouer

Lancer `pokefirered.exe` ou `pokeleafgreen.exe` depuis un dossier où il peut écrire.
Les deux jeux peuvent partager un dossier : sauvegardes séparées (`firered.sav`,
`leafgreen.sav`, 128 Kio, même format que la cartouche), `config.ini` commun.
Comme sur la cartouche, il n'y a pas d'horloge.

| GBA | Clavier |
|---|---|
| A / B | X / Z |
| Start / Select | Entrée / Retour arrière |
| Croix | Flèches |
| L / R | A / S (L/R ouvrent aussi l'aide du jeu) |
| Plein écran | F11 ou Alt+Entrée |

Touches, taille de fenêtre, plein écran et volume se règlent dans **`config.ini`**,
créé à côté de l'exe au premier lancement (noms de touches SDL, plusieurs par bouton
séparés par des virgules). Une manette reconnue par SDL fonctionne aussi.

## Variables d'environnement (tests et débogage)

| Variable | Effet |
|---|---|
| `PFR_SCALE=n` | taille de la fenêtre (×3 par défaut) |
| `PFR_MUTE=1` | sans son |
| `PFR_SAVE=f` | autre fichier de sauvegarde |
| `PFR_HEADLESS=1`, `PFR_FAST=1` | sans fenêtre, sans limite de vitesse |
| `PFR_EXIT_AFTER=N` | quitter après N images |
| `PFR_INPUT=2600-2604:START,3000-7600/30:A` | touches scriptées (`/P` = appui toutes les P images) |
| `PFR_SHOT=900:titre.bmp` | capture d'une image |
| `PFR_WAV=son.wav` | enregistrer le son |
| `PFR_TRACE=1` | journal des changements de carte / position / contrôle |
| `PFR_TEST=givemon@N,battle@N` | donner un Salamèche, lancer un combat sauvage |
| `PFR_REGS=N`, `PFR_PEEK=ADDR:LEN` | registres, VRAM, tâches ; mémoire brute (adresses de `nm`) |
| `PFR_HANG_EXIT=1` | quitter si le jeu se bloque (le chien de garde affiche la pile) |
| `PFR_WATCH=0xADDR` (+`PFR_WATCH_LEN`, `PFR_WATCH_ALL`) | point d'arrêt matériel en écriture (Windows) |

Plantages et blocages s'affichent avec une pile d'adresses ;
`python src/platform_pc/scripts/symbolize.py journal.txt` les traduit en fonctions/lignes.

## Particularités

- Les lectures/écritures à travers un pointeur nul, inoffensives sur GBA (l'adresse 0 est
  le BIOS, la zone suivante n'est pas câblée), sont reproduites comme sur GBA et
  signalées une fois dans le journal (`[pfr:nullread]`) : ce sont des comportements du
  jeu d'origine (Rouge Feu lit même sa sauvegarde ainsi au démarrage).
- Rouge Feu a été vendu avec ses vérifications de débogage (`AGB_ASSERT`) actives : leurs
  messages, invisibles sur console, vont dans le journal (`agbprint:`).
- Pas de câble link ni d'adaptateur sans-fil : les menus correspondants répondent
  « pas de connexion », comme une cartouche seule.

Journal détaillé du portage : [PORTING_ROADMAP.md](PORTING_ROADMAP.md).
