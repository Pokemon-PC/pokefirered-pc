/*
 * rom_pc.c - finds the player's own ROM of this game (FireRed or LeafGreen).
 *
 * The PC port does not need a ROM to run: everything is built from the
 * decomp. This only locates the player's cartridge dump (for a launcher, and
 * for features that would read data from it) and logs it at startup.
 *
 * Search order, first hit wins:
 *   1. an explicit file: --rom FILE, PFR_ROM, config.ini [game] rom
 *   2. the executable's folder
 *   3. the ROM folder: --rom-dir DIR, PFR_ROM_DIR, config.ini [game] rom_dir
 * In a folder, every *.gba is opened and chosen by its header, never by its
 * name: game code BPR? (FireRed) or BPG? (LeafGreen), valid header checksum.
 * Language (4th letter of the code): the build's language first (English),
 * then French, then any other.
 */
#include "pc_hal.h"
#include "platform_pc.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#endif

#if defined(LEAFGREEN)
#define GAME_CODE3 "BPG"
#else
#define GAME_CODE3 "BPR"
#endif

static char s_cli_rom[1024], s_cli_dir[1024];
static char s_found[1100];
static char s_lang;
static bool s_searched;

void pc_rom_set_cli(const char *rom, const char *dir) {
    snprintf(s_cli_rom, sizeof s_cli_rom, "%s", rom ? rom : "");
    snprintf(s_cli_dir, sizeof s_cli_dir, "%s", dir ? dir : "");
    s_searched = false;
}

/* Language letter of path if it is a ROM of this game with a valid header, else 0. */
char pc_rom_identify(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    unsigned char h[0xC0];
    size_t n = fread(h, 1, sizeof h, f);
    fclose(f);
    if (n != sizeof h || h[0xB2] != 0x96 || memcmp(h + 0xAC, GAME_CODE3, 3) != 0) {
        return 0;
    }
    unsigned char sum = 0;
    for (int i = 0xA0; i < 0xBD; i++) {
        sum = (unsigned char)(sum - h[i]);
    }
    if ((unsigned char)(sum - 0x19) != h[0xBD]) {
        return 0;
    }
    return (char)h[0xAF];
}

static int lang_rank(char l) {
#if defined(ENGLISH)
    if (l == 'E') return 0;
#endif
    return l == 'F' ? 1 : l == 'E' ? 2 : 3;
}

static bool is_gba(const char *name) {
    size_t l = strlen(name);
    return l > 4 && SDL_strcasecmp(name + l - 4, ".gba") == 0;
}

static void consider(const char *dir, const char *name, char *best, size_t n, int *best_rank, char *best_lang) {
    char path[1100];
#if defined(_WIN32)
    snprintf(path, sizeof path, "%s\\%s", dir, name);
#else
    snprintf(path, sizeof path, "%s/%s", dir, name);
#endif
    char l = pc_rom_identify(path);
    if (l != 0 && lang_rank(l) < *best_rank) {
        *best_rank = lang_rank(l);
        *best_lang = l;
        snprintf(best, n, "%s", path);
    }
}

/* Best ROM of this game among the *.gba files of dir (flat, no subfolders). */
bool pc_rom_find_in_dir(const char *dir, char *found, size_t n, char *lang) {
    int best_rank = 99;
    char best_lang = 0;
#if defined(_WIN32)
    char pattern[1100];
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && is_gba(fd.cFileName)) {
            consider(dir, fd.cFileName, found, n, &best_rank, &best_lang);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (d == NULL) {
        return false;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (is_gba(e->d_name)) {
            consider(dir, e->d_name, found, n, &best_rank, &best_lang);
        }
    }
    closedir(d);
#endif
    if (lang) {
        *lang = best_lang;
    }
    return best_lang != 0;
}

static const char *first_set(const char *a, const char *env, const char *key) {
    if (a && *a) {
        return a;
    }
    const char *v = getenv(env);
    if (v && *v) {
        return v;
    }
    v = pc_config_get("game", key, NULL);
    if (v && *v) {
        return v;
    }
    v = pc_config_get("", key, NULL);   /* also accepted before any [section] */
    return v && *v ? v : NULL;
}

const char *pc_rom_path(void) {
    if (s_searched) {
        return s_found[0] ? s_found : NULL;
    }
    s_searched = true;
    s_found[0] = '\0';
    s_lang = 0;

    const char *rom = first_set(s_cli_rom, "PFR_ROM", "rom");
    if (rom) {
        s_lang = pc_rom_identify(rom);
        if (s_lang) {
            snprintf(s_found, sizeof s_found, "%s", rom);
            return s_found;
        }
        pc_log("rom: %s is not a " PC_GAME_TITLE " ROM (game code " GAME_CODE3 "?), searching folders", rom);
    }
    char *base = SDL_GetBasePath();
    if (base) {
        size_t l = strlen(base);
        while (l > 1 && (base[l - 1] == '/' || base[l - 1] == '\\')) {
            base[--l] = '\0';
        }
        bool ok = pc_rom_find_in_dir(base, s_found, sizeof s_found, &s_lang);
        SDL_free(base);
        if (ok) {
            return s_found;
        }
    }
    const char *dir = first_set(s_cli_dir, "PFR_ROM_DIR", "rom_dir");
    if (dir && pc_rom_find_in_dir(dir, s_found, sizeof s_found, &s_lang)) {
        return s_found;
    }
    s_found[0] = '\0';
    return NULL;
}

char pc_rom_language(void) {
    return pc_rom_path() ? s_lang : 0;
}

void pc_rom_log(void) {
    const char *p = pc_rom_path();
    if (p) {
        pc_log("rom: %s (" GAME_CODE3 "%c)", p, s_lang);
    } else {
        pc_log("rom: no " PC_GAME_TITLE " ROM found (not needed to play; set rom_dir in config.ini)");
    }
}
