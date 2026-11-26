# Feuille de route — Pokémon Rouge Feu / Vert Feuille en exécutable PC natif

Objectif : `pokefirered.exe` et `pokeleafgreen.exe` (Windows, puis Linux) compilés
depuis la décomp pret/pokefirered, sans émulateur, avec CMake + Clang + SDL2, en
gardant la ROM GBA « matching » (`make compare` doit toujours donner `OK`).

Méthode : celle du port Émeraude (`pokeemerald-master`, même moteur GBA), dont la
couche `src/platform_pc/`, les scripts, le CMake et les tests sont repris.

## Règles

1. **ROM GBA matching** : tout changement hors de `src/platform_pc/` est sous
   `#ifdef PLATFORM_PC` (code d'origine dans `#else`), ou l'annotation `PC_PTR32`
   (vide sur GBA). Le `Makefile` GBA ignore `src/platform_pc/`. Vérification :
   `python src/platform_pc/scripts/check_gba_sources.py` (base : commit pret
   `ce6406d8c`, dernier avant le port).
2. **Aucune sauvegarde ni ROM dans le dépôt** ; pas de ROM nécessaire : toutes les
   données (anglais US) se construisent depuis le dépôt.
3. **Le code du jeu reste le code du jeu** : on émule le matériel GBA.
4. **Prouver avant d'affirmer** : capture d'image, journal ou autotest.

## Décisions

| Sujet | Choix | Raison |
|---|---|---|
| Couche PC | copie de celle d'Émeraude, préfixe `PFR_` (variables d'environnement, CMake) | même console, même moteur (MP2K, BIOS, DMA, Flash) |
| Versions | `-DPFR_GAME_VERSION=FIRERED\|LEAFGREEN` → `pokefirered` / `pokeleafgreen`, un dossier de build chacun | comme `GAME_VERSION` de `config.mk` et HeartGold/SoulSilver |
| Ce qui dépend de la version | `platform_pc.h` : titre de fenêtre, sauvegarde par défaut (`firered.sav` / `leafgreen.sav`), code jeu simulé `BPRE`/`BPGE` | les deux jeux peuvent partager un dossier (et `config.ini`) |
| Révision / langue | révision 0, anglais (`REVISION=0`, `ENGLISH`) | défaut de `config.mk` |
| Horloge | aucune : la cartouche Rouge Feu/Vert Feuille n'a pas de RTC (`rtc_pc.c` retiré) | |
| Données | générées une fois pour les deux versions (les deux jeux de graphismes sont dans le dépôt, le code choisit par `#if`) | |

## Audit M0 : ce qui change par rapport à Émeraude

- **Même matériel, même moteur** : `m4a_1.s` (mixeur MP2K, déjà en C dans
  `sound_pc.c`), `crt0.s` (même ordre d'IRQ `gIntrTableTemplate`), `libagbsyscall`,
  `agb_flash*.c` (+ `agb_flash_le.c`, remplacé aussi par `flash_pc.c`), `librfu_*`,
  `multiboot.c`, `libgcnmultiboot.s`, `rom_header.s`. 283 fichiers C, 14 fichiers
  `data/*.s`, 347 musiques MIDI.
- **Outils plus anciens** que ceux d'Émeraude : `preproc` sans `-g`, pas d'`INCGFX`
  (les `INCBIN` visent directement `graphics/….4bpp.lz`, générés à côté des PNG) ;
  `gen_assets.py` et `pp_c.py` adaptés.
- **Version dans l'assembleur** : les données `.s` testent `.if FIRERED` (symboles
  `--defsym` de `ASFLAGS`), le C `#if defined(FIRERED)` : CMake passe les deux.
- `gHeap` vient du script de lien (début de l'EWRAM) : sur PC `#define gHeap
  ((u8 *)EWRAM_START)`, comme Émeraude.
- `rom_header.s` : `RomHeaderGameCode` est un `const char[]` dans Rouge Feu.
- `ProgramFlashSectorAndVerifyNBytes` programme le secteur entier (comme
  `agb_flash.c` de Rouge Feu) ; `ReadFlash` prend un `void *`.
- Correctifs 64 bits d'Émeraude reportés : tables de pointeurs `.4byte` en
  `PC_PTR32` (scripts de combat, animations, `gStdScripts`, `gSpecialVars`,
  cartes…), tables de fonctions en `.8byte` (`gScriptCmdTable`, `gSpecials`,
  Cadeau Mystère), `LoadWordFromTwoHalfwords` vers un pointeur (5 sites),
  `pcDataOverflow` des tâches, tampon `librfu`, boucles sur registres
  (`pc_advance_line`), `WaitForVBlank`, `SetGpuReg`, secteur Flash effacé.
- Rouge Feu **lit sa sauvegarde au démarrage avant d'avoir placé ses blocs de
  sauvegarde** (`CB2_InitCopyrightScreenAfterBootup` → `LoadGameSave` alors que
  `gSaveBlock*Ptr`/`gPokemonStoragePtr` sont NULL) : sur GBA les écritures tombent
  dans le BIOS et la zone non câblée (0x4000-0x1FFFFFF) et sont ignorées. Sur PC :
  la zone est réservée sans accès (`hw_pc.c`) et l'émulation des accès via NULL
  (`nullread_pc.c`) couvre désormais 0-0x1FFFFFF.
- Rouge Feu garde ses `AGB_ASSERT` (pas de `NDEBUG` dans la ROM commerciale) :
  `isagbprn.c` (ARM) remplacé par `isagbprn_pc.c`, messages dans le journal.

## Jalons

| Jalon | Contenu | Critère de sortie | État |
|---|---|---|---|
| M0 Audit | différences avec Émeraude | section ci-dessus | ✅ |
| M1 Outils + données | outils de la décomp compilés, ~8 500 fichiers générés | `gen_assets.py` : « données générées » | ✅ (sous WSL, voir journal) |
| M2 Couche matériel | HAL d'Émeraude adaptée | autotest CTest vert | ✅ Linux (55/55) |
| M3 Démarrage | tout le jeu compilé, copyright → intro → titre | captures | ✅ Linux |
| M4 Rendu | intro, titre, monde justes | captures | ✅ Linux |
| M5 Son | MP2K en C (repris) | musique de l'intro non muette | ✅ Linux (écoute à faire) |
| M6 Jeu | nouvelle partie, monde, combat, menus, sauvegarde/reprise, `config.ini`, paquet | tests CTest | ✅ Linux, Rouge Feu et Vert Feuille (4/4 chacun) |
| M7 Linux / Windows | build + tests | ctest vert | ✅ Linux et Windows (4/4 chaque version) ; CI écrite, pas exécutée |
| M8 Langues | anglais (décomp) **et** français (ROM FR du joueur), `language = auto\|fr\|en`, `--lang`, `PFR_LANG` | les deux langues testées | ✅ Rouge Feu (`game_french`, Windows et Linux) ; ⏳ Vert Feuille : ROM FR reçue, table à construire (voir journal) |

## À faire par l'utilisateur

- Écoute du son, ressenti, manette réelle.
- Vérifier le matching GBA pour de vrai (devkitARM/agbcc, avec accord).

## Journal

- **2026-11-25** — Début du port. Couche PC d'Émeraude copiée (`src/platform_pc/`,
  `cmake/`, `CMakeLists.txt`), préfixe `PFR_`, langue française retirée (pas de
  table pour Rouge Feu), RTC retirée. Option de version FIRERED/LEAFGREEN.
  - AVG bloque `preproc.exe` dès sa première exécution : outils et données faits
    sous **WSL** (binaires Linux à côté des `.exe` dans `tools/*/`, données
    identiques quel que soit l'OS). `gen_assets.py` : 303 sources, 8 496 fichiers.
  - Correctifs du jeu reportés (voir audit) ; `check_gba_sources.py` : 0 différence.
  - Autotest de la couche matériel sous Linux : 53 vérifications, 0 échec.
- **2026-11-25 (suite)** — Le jeu entier compile et tourne sous Linux (WSL, Clang).
  - Compilation : `LaunchBattleAnimation` (définition en `PC_PTR32`), `gStdScripts + i`
    (scrcmd.c), `MusicPlayerJumpTableCopy` (`swi 0x2A` → `MPlayJumpTableCopy`),
    `AgbMain` (asm « MODERN » d'effacement de la RAM → `RegisterRamReset`),
    `isagbprn.c` exclu, polices `.fwlatfont/.hwlatfont` ajoutées à `gen_assets.py`.
  - Assembleur x86 : `(MAX_COINS + 1) - 50` en argument de macro est coupé en deux par
    GNU as x86 (pas par celui d'ARM) → `pp_s.py` colle l'opérateur à la parenthèse
    (lignes sans chaîne). Classe : expressions espacées après `)` dans les scripts.
  - **Vu en jeu** (captures) : copyright → Game Freak → combat Ectoplasma/Nidorino →
    écran titre (Dracaufeu, flammes) → guide des commandes → Prof. Chen → noms →
    chambre (4.1) → escalier (warp « descente à gauche » : appuyer à gauche) → salon
    (4.0) → Bourg Palette (3.0) → combat sauvage (Salamèche contre Rattata, gagné) →
    retour sur la carte ; menu Start, sac, carte de dresseur, SAUVER ; CONTINUER →
    « Previously on your quest… » → même position ; équipe, résumé (2 pages).
  - Plantage à la reprise (écriture à 0x7C00) : voir l'audit (sauvegarde lue via NULL).
    Autotest : + zone non câblée (55 vérifications).
  - Accès via NULL rencontrés (comportements d'origine, signalés une fois) : écran de
    nom (`VBlankCB_NamingScreen`, `SpriteCB_Underscore`, `CurrentPageToKeyboardId`),
    transition de combat (`VBlankCB_Slice`), chargement de sauvegarde au démarrage.
  - Tests `ctest` (`smoke_test.py` réécrit pour Rouge Feu) : `hal_autotest`,
    `game_smoke` (titre tenu 95 %, musique de l'intro RMS ≈ 2 700), `game_world`,
    `game_save` — **4/4 Rouge Feu, 4/4 Vert Feuille** (écran titre Florizarre,
    `leafgreen.sav`).
  - `README_PC.md`, `package.py` (les deux exe dans un zip), CI `.github/workflows/pc.yml`
    (Linux + Windows × 2 versions ; écrite, pas exécutée).
  - Reste : écoute du son, fenêtre/son réels sous Linux, parcours d'histoire plus
    long (labo du Prof. Chen, premier rival).
- **2026-11-25 (Windows)** — Exception AVG ajoutée par l'utilisateur : les exe se lancent.
  - Édition de liens PE : `data/mystery_event_msg.s` utilise `specialvar … X` sans
    inclure `data/specials.inc` → champ 16 bits `SPECIAL_X` laissé à l'éditeur de liens
    (relocation 16 bits inexistante en PE/COFF). `pp_s.py` (COFF seulement) définit
    alors tous les `SPECIAL_x` d'après l'ordre de `def_special` (même numéro).
  - **Windows (llvm-mingw, Clang)** : `pokefirered.exe` 4/4, `pokeleafgreen.exe` 4/4 ;
    captures vérifiées (Bourg Palette, combat, chambre, salon). Paquet
    `dist/pokefirered-pc-ce6406d8c.zip` (13,4 Mio, les deux exe).
    `check_gba_sources.py` : 0 différence.
  - Commit `9f95c387a` (branche `pc-port`), publié sur `Pokemon-PC/pokefirered-pc`.
- **2026-11-25 (ROM)** — Les ROM sont réunies dans un dossier commun, à plat :
  `D:\LUC\pokemonall\roms` (Rouge Feu FR seulement ; pas de ROM Vert Feuille).
  - Le port n'utilise toujours **aucune** ROM (tout vient de la décomp, en anglais) ;
    `rom_pc.c` sait la retrouver, pour le lanceur et de futures fonctions (textes
    français) : `--rom` / `PFR_ROM` / `[game] rom`, puis dossier de l'exe, puis
    `--rom-dir` / `PFR_ROM_DIR` / `[game] rom_dir` (vide par défaut). Choix par en-tête
    (`BPR?`/`BPG?`, somme de contrôle), langue du build puis français puis autres.
    Journal au démarrage : `rom: … (BPRF)`.
  - Autotest : + 5 vérifications (fausses en-têtes aux noms trompeurs : choix par
    en-tête, autre version refusée, somme fausse refusée, ordre des langues,
    `PFR_ROM_DIR`). `config.ini` locaux de `build-pc`/`build-lg` : `rom_dir` réglé.
  - Vérifié : `pokefirered.exe` trouve `Pokemon - Version Rouge Feu (France).gba`
    (BPRF) via `rom_dir`, `PFR_ROM_DIR` et `--rom-dir` ; `pokeleafgreen.exe` : « no
    ROM found » (aucune ROM Vert Feuille), le jeu tourne quand même.
- **2026-11-26 — Français (Rouge Feu)**, d'après la méthode d'Émeraude/Rubis
  (skill `nds-decomp-pc-port`, réf. 09 §4) : textes tirés de la ROM FR du joueur au
  lancement, aucun texte de Nintendo dans le dépôt (seulement des positions).
  - Outils `src/platform_pc/scripts/lang/` repris de Rubis et adaptés : ROM `BPRF`
    rév. 0 (sha1 `fc663907…`, CRC-32 `5DC668F6`), ordre de liaison `ld_script.ld`,
    défisym `FIRERED`, tables de Rouge Feu (`find_fr_tables.py` : 8 tables, toutes
    gagnées nettement, 100 % des entrées dans leur champ ; vérifiées en décodant).
  - `build_fr_map.py` → `lang/fr_bprf.tsv` : **7 577 / 9 279 textes (81,7 %)**.
    Échantillons relus (40 paires par méthode) : pointeurs de scripts, positions et
    tables ≈ 100 % justes ; alignement ≈ 1 faute sur 6 → règles ajoutées :
    - une paire alignée n'est gardée qu'encadrée par deux paires sûres (l'exception
      `strings.c` de Rubis ne tient pas ici) ; alignements dans les scripts écartés ;
    - un même texte FR donné à plusieurs textes anglais différents → écarté (Salle
      Union : table mal lue) ; paires relues fausses (`REVIEWED_WRONG` : accueils du
      Club Câble décalés d'un texte) ;
    - jamais de bytecode : étiquettes `AI_`/`BattleScript_`… et unités d'un caractère
      (`AI_CV_Trick_ChoiceEffects` aurait corrompu le script d'IA) ;
    - habitats du Pokédex : table FR `{texte, id}` avec un titre en tête → entrée
      suivante (+8) ; noms de lieux : table de pointeurs FR trouvée par « BOURG
      PALETTE » + « JADIELLE » (109/109) ; préfixes de combat « Wild/Foe/Ally » →
      « sauvage/ennemi/ami » (8 chaînes consécutives, vérifiées) ;
    - 302 libellés courts relus un par un (`lang/fr_bprf_reviewed.txt`, noms de
      symboles seulement) ; alias pour l'encadré de sauvegarde, le « CANCEL » du sac
      et « COMMANDES » (même texte anglais, même rôle).
  - Build : `pp_c.py`/`pp_s.py --lang-map` (place réservée + registre, section
    `.rdata$pfrlang1` / `pfr_lang`), seulement si la table existe (Rouge Feu).
  - Exécution : `lang_pc.c` (ROM cherchée par `rom_pc.c` avec le filtre de langue
    `F`, vérifiée par CRC, copie **avant** l'instantané du reset doux),
    `lang_tables_pc.c` (8 tables de noms). `battle_message.c` (sous `PLATFORM_PC`) :
    en français le nom puis « sauvage » / « ennemi », comme le jeu français.
  - Choix : `config.ini [game] language = auto|en|fr` (défaut `auto`), `--lang`,
    `PFR_LANG`. Les tests anglais forcent `PFR_LANG=en` (touches calées sur le texte
    anglais ; avec `rom_dir` réglé, `auto` passerait en français).
  - **Vu en français** (captures) : discours du Prof. Chen, guide des commandes, chambre,
    salon, Bourg Palette, « Un RATTATA sauvage… », « RATTATA sauvage utilise
    MIMI-QUEUE! », « Que doit faire SALAMECHE? », ATTAQUE/SAC/POKéMON/FUITE, menu
    Start (SAC/SAUVER/OPTIONS/RETOUR), sac (« FERMER LE SAC »), carte de dresseur
    (NOM, ARGENT, NºID), sauvegarde (« Voulez-vous sauvegarder », « SAUVEGARDE EN
    COURS… », encadré BOURG PALETTE).
  - Le français est plus long : le joueur a la main dans sa chambre vers l'image
    7 908 (≈ 190 plus tard) → parcours du test français décalé de 250 images.
  - Test `game_french` (`-DPFR_ROM_DIR=…`) : ROM prise dans `rom_dir`, ≥ 5 000 textes,
    8 tables à ≥ 90 %, Bourg Palette atteint, combat lancé, l'image du Prof. Chen
    diffère en anglais. **Windows : Rouge Feu 5/5, Vert Feuille 4/4 ; Linux (WSL) :
    Rouge Feu 5/5** (build par `tools/heavy.sh`, file WSL, 5 tâches). `check_gba_sources.py` : 0 différence.
  - Restent anglais : ~18 % de textes non placés avec certitude (libellés courts,
    textes japonais inutilisés de la décomp, « NEXT » du guide déclaré `ALIGNED(4)`),
    les textes dessinés (logo, « TRAINER CARD »), « TIME » de la carte.
  - Vert Feuille : pas de ROM FR → `pokeleafgreen` reste anglais et le dit
    (« no French text map for Pokemon LeafGreen yet »).
- **2026-11-26 (fin) — Vert Feuille français : arrêté ici** (limite d'usage de
  l'utilisateur ; commit demandé à un point propre).
  - ROM reçue et rangée : `roms/Pokemon - Version Vert Feuille (France).gba` (`BPGF`
    rév. 0, sha1 `4b5758c1…`, CRC-32 `BA3285E3`).
  - Fait : `find_fr_tables.py` → 8 tables trouvées (mêmes votes que Rouge Feu, noms
    vérifiés en décodant) et écrites dans `lang_tables_pc.c` ; `lang_pc.c` connaît la
    ROM ; `build_fr_map.py` connaît son sha1 ; `lang/fr_bpgf_reviewed.txt` = la liste
    relue de Rouge Feu (textes communs aux deux versions, à confirmer par
    échantillons).
  - Garde-fou : le français de Vert Feuille ne s'active que si `lang/fr_bpgf.tsv`
    existe (CMake définit `PFR_HAVE_LANG_MAP`). Ce fichier n'est **pas encore
    construit** : `pokeleafgreen` reste en anglais (« no French text map… »), vérifié
    (game_smoke + autotest verts, ROM trouvée mais anglais).
  - **Reste à faire** :
    1. `tools/heavy.sh "rouge-feu: table FR Vert Feuille" python
       src/platform_pc/scripts/lang/build_fr_map.py "…/Vert Feuille (France).gba" build-lg`
       (build anglais `build-lg` à jour) ;
    2. relire des échantillons (méthodes `ptr`/`pos`/`table`/`dp`) et les étapes
       propres à Rouge Feu (habitats décalés, noms de lieux « BOURG PALETTE » +
       « JADIELLE », préfixes sauvage/ennemi, alias) — les adapter si besoin ;
    3. `cmake -B build-lg -DPFR_ROM_DIR=…`, recompiler (via `heavy.sh`), `ctest` (le
       test `game_french` s'ajoute tout seul), captures en français ;
    4. Linux (`HEAVY_POOL=wsl`, `build-tools/wsl_fr_test.sh`), README, commit.
