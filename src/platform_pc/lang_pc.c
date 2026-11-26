/*
 * lang_pc.c - choice of language: English (built in) or French (from the
 * player's own French ROM, never shipped).
 *
 * Each English string listed in src/platform_pc/lang/fr_bprf.tsv (FireRed)
 * or fr_bpgf.tsv (LeafGreen) was given room for its French version at build time, and an entry in the
 * registry section { string, French offset, French length }
 * (scripts/lang/lang_build.py). With French chosen, the player's ROM
 * "Pokemon - Version Rouge Feu (France)" (BPRF) or "Version Vert Feuille
 * (France)" (BPGF), revision 0 - the one of this executable - is looked for,
 * checked (CRC-32), and the French strings are copied over the English ones
 * before the game starts; name tables follow (lang_tables_pc.c).
 *
 * config.ini [game] language = auto | en | fr   (auto: French if the ROM is
 * found). The ROM is searched like rom_pc.c does: [game] rom, the
 * executable's folder, [game] rom_dir, the working folder - chosen by header
 * (game code BPRF / BPGF), never by name. PFR_LANG / PFR_ROM / PFR_ROM_DIR (or
 * --lang / --rom / --rom-dir) override the config.
 */
#include "pc_hal.h"
#include "platform_pc.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

/* The French ROM of this game (16 MiB). */
#define FR_ROM_SIZE  0x1000000u
#if defined(LEAFGREEN)
/* Only once its French map (lang/fr_bpgf.tsv) is in the build: CMake
 * defines PFR_HAVE_LANG_MAP then. Without it, English. */
#if defined(PFR_HAVE_LANG_MAP)
#define FR_ROM_LANG_MAP 1
#else
#define FR_ROM_LANG_MAP 0
#endif
#define FR_ROM_CODE  "BPGF"
#define FR_ROM_CRC32 0xBA3285E3u    /* Pokemon - Version Vert Feuille (France), rev 0 */
#define FR_ROM_NAME  "French LeafGreen"
#else
#define FR_ROM_LANG_MAP 1
#define FR_ROM_CODE  "BPRF"
#define FR_ROM_CRC32 0x5DC668F6u    /* Pokemon - Version Rouge Feu (France), rev 0 */
#define FR_ROM_NAME  "French FireRed"
#endif

struct PfrLangText {
    const unsigned char *text;
    unsigned int frOffset, frLength;
};

/* Registry bounds: one section, entries from every game file in between. */
#if defined(_WIN32)
__attribute__((section(".rdata$pfrlang0"), used, aligned(8))) static const struct PfrLangText s_lang_begin[1];
__attribute__((section(".rdata$pfrlang2"), used, aligned(8))) static const struct PfrLangText s_lang_end[1];
#define LANG_BEGIN (s_lang_begin + 1)
#define LANG_END   (s_lang_end)
#elif defined(__APPLE__)
extern const struct PfrLangText s_lang_start[] __asm("section$start$__DATA$__pfrlang");
extern const struct PfrLangText s_lang_stop[] __asm("section$end$__DATA$__pfrlang");
#define LANG_BEGIN s_lang_start
#define LANG_END   s_lang_stop
#else
extern const struct PfrLangText __start_pfr_lang[] __attribute__((weak));
extern const struct PfrLangText __stop_pfr_lang[] __attribute__((weak));
#define LANG_BEGIN __start_pfr_lang
#define LANG_END   __stop_pfr_lang
#endif

static char s_lang[8] = "en";
static char s_cli_lang[8];

const char *pc_lang_current(void) {
    return s_lang;
}

void pc_lang_set_cli(const char *lang) {
    snprintf(s_cli_lang, sizeof s_cli_lang, "%s", lang ? lang : "");
}

#if FR_ROM_LANG_MAP
static uint32_t crc32_of(const uint8_t *p, size_t n) {
    static uint32_t table[256];
    if (table[1] == 0) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            table[i] = c;
        }
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

static uint8_t *load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    uint8_t *rom = malloc(FR_ROM_SIZE);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (rom == NULL || size != (long)FR_ROM_SIZE || fread(rom, 1, FR_ROM_SIZE, f) != FR_ROM_SIZE) {
        free(rom);
        rom = NULL;
    }
    fclose(f);
    return rom;
}
#endif

/* Makes the page(s) of [p, p+n) writable (strings live in read-only data). */
void pc_lang_make_writable(const void *p, size_t n) {
    static uintptr_t last_page = 0;
#if defined(_WIN32)
    const uintptr_t page = 4096;
#else
    const uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
#endif
    uintptr_t a = (uintptr_t)p & ~(page - 1);
    uintptr_t end = ((uintptr_t)p + n + page - 1) & ~(page - 1);
    for (; a < end; a += page) {
        if (a == last_page) {
            continue;
        }
#if defined(_WIN32)
        DWORD old;
        VirtualProtect((void *)a, page, PAGE_READWRITE, &old);
#else
        mprotect((void *)a, page, PROT_READ | PROT_WRITE);
#endif
        last_page = a;
    }
}

void pc_lang_tables_apply(const uint8_t *rom);   /* lang_tables_pc.c */

void pc_lang_apply(void) {
    const char *want = s_cli_lang[0] ? s_cli_lang : getenv("PFR_LANG");
    if (want == NULL || *want == '\0') {
        want = pc_config_get("game", "language", "auto");
    }
    if (SDL_strcasecmp(want, "en") == 0) {
        pc_log("lang: English");
        return;
    }
    bool asked = SDL_strcasecmp(want, "fr") == 0;
#if !FR_ROM_LANG_MAP
    pc_log("lang: English (%s: no French text map for " PC_GAME_TITLE " yet)", asked ? "French asked" : "auto");
#else
    char path[1100];
    if (!pc_rom_find_language('F', path, sizeof path)) {
        if (asked) {
            pc_log("lang: French asked, but no " FR_ROM_NAME " ROM (" FR_ROM_CODE ") found next to the game"
                   " or in rom_dir: English. Put your own ROM (*.gba) next to the executable, or set"
                   " [game] rom_dir (or rom) in config.ini.");
        } else {
            pc_log("lang: English (no French ROM found)");
        }
        return;
    }
    uint8_t *rom = load_rom(path);
    if (rom == NULL) {
        pc_log("lang: cannot read %s (expected 16 MiB): English", path);
        return;
    }
    uint32_t crc = crc32_of(rom, FR_ROM_SIZE);
    if (crc != FR_ROM_CRC32) {
        pc_log("lang: %s is a " FR_ROM_NAME " ROM but not the expected one (CRC %08X, want %08X): English",
               path, (unsigned)crc, (unsigned)FR_ROM_CRC32);
        free(rom);
        return;
    }
    int applied = 0, skipped = 0;
    for (const struct PfrLangText *e = LANG_BEGIN; e < LANG_END; e++) {
        if (e->text == NULL) {
            continue;                      /* section padding */
        }
        if (e->frOffset + e->frLength > FR_ROM_SIZE || e->frLength == 0
            || rom[e->frOffset + e->frLength - 1] != 0xFF) {
            skipped++;
            continue;
        }
        pc_lang_make_writable(e->text, e->frLength);
        memcpy((void *)e->text, rom + e->frOffset, e->frLength);
        applied++;
    }
    pc_lang_tables_apply(rom);
    free(rom);
    snprintf(s_lang, sizeof s_lang, "fr");
    pc_log("lang: French from %s (%d strings%s)", path, applied, skipped ? ", some skipped" : "");
#endif
}

int pc_lang_is_french(void) {
    return s_lang[0] == 'f';
}
