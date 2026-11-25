/*
 * link_pc.c - link cable, wireless adapter, multiboot: "cable unplugged".
 *
 * The C parts of the link code (link.c, link_rfu_*.c, librfu_*.c...) are
 * compiled as-is: with nothing answering on the serial port they report no
 * partner, and the game shows its own "no connection" messages. Only the
 * ARM-only pieces left out of the build are replaced here:
 *   - src/librfu_intr.c  (IntrSIO32: serial interrupt of the wireless adapter)
 *   - src/multiboot.c    (sending a program to another GBA)
 *   - src/libgcnmultiboot.s (GameCube link for Pokemon Colosseum)
 *   - src/rom_header.s   (the ROM header fields the game reads)
 */
#include <string.h>

#include "platform_pc.h"

#include "global.h"
#include "gba/multiboot.h"
#include "libgcnmultiboot.h"
#include "librfu.h"
#include "main.h"
#include "constants/items.h"
#include "multiboot.h"

static void note_once(const char *what) {
    pc_log("link: %s (no link cable/adapter on PC)", what);
}

/* ---- rom_header.s ------------------------------------------------------- */
#ifdef LEAFGREEN
const char RomHeaderGameCode[GAME_CODE_LENGTH] = { 'B', 'P', 'G', 'E' };
#else
const char RomHeaderGameCode[GAME_CODE_LENGTH] = { 'B', 'P', 'R', 'E' };
#endif
const char RomHeaderSoftwareVersion = 0;   /* REVISION */

/* ---- librfu_intr.c ------------------------------------------------------- */
void IntrSIO32(void) {}

/* ---- multiboot.c: no other GBA ever answers ----------------------------- */
void MultiBootInit(struct MultiBootParam *mp) {
    static int once;
    if (!once) {
        once = 1;
        note_once("multiboot");
    }
    memset(mp, 0, sizeof(*mp));
}

int MultiBootMain(struct MultiBootParam *mp) {
    (void)mp;
    return 0;   /* still waiting for a client */
}

void MultiBootStartProbe(struct MultiBootParam *mp) {
    (void)mp;
}

void MultiBootStartMaster(struct MultiBootParam *mp, const u8 *srcp, int length, u8 palette_color, s8 palette_speed) {
    (void)mp;
    (void)srcp;
    (void)length;
    (void)palette_color;
    (void)palette_speed;
}

bool32 MultiBootCheckComplete(struct MultiBootParam *mp) {
    (void)mp;
    return 0;
}

/* ---- libgcnmultiboot.s: no GameCube on the other end -------------------- */
void GameCubeMultiBoot_Init(struct GcmbStruct *pStruct) {
    memset(pStruct, 0, sizeof(*pStruct));
}
void GameCubeMultiBoot_Main(struct GcmbStruct *pStruct) { (void)pStruct; }
void GameCubeMultiBoot_ExecuteProgram(struct GcmbStruct *pStruct) { (void)pStruct; }
void GameCubeMultiBoot_HandleSerialInterrupt(struct GcmbStruct *pStruct) { (void)pStruct; }
void GameCubeMultiBoot_Quit(void) {}

/* ---- constants the data files reference without defining ---------------
 * asm/macros/ .inc files alias C constants (.equiv connection_up, CONNECTION_NORTH;
 * enum_start ITEM_TM01...). Files that include the macros but not the C
 * header keep those aliases unresolved; ARM ELF gas drops unused ones, PE/COFF
 * gas emits them as external references. Define them, with their real values
 * (none is actually used by those files). */
#define DEFINE_ABS(name, value) __asm__ volatile(".globl " #name "\n.set " #name ", %c0" ::"i"(value))

__attribute__((used)) static void pfr_define_asm_constants(void) {
    DEFINE_ABS(CONNECTION_SOUTH, CONNECTION_SOUTH);
    DEFINE_ABS(CONNECTION_NORTH, CONNECTION_NORTH);
    DEFINE_ABS(CONNECTION_WEST, CONNECTION_WEST);
    DEFINE_ABS(CONNECTION_EAST, CONNECTION_EAST);
    DEFINE_ABS(CONNECTION_DIVE, CONNECTION_DIVE);
    DEFINE_ABS(CONNECTION_EMERGE, CONNECTION_EMERGE);
    DEFINE_ABS(ITEM_TM01, ITEM_TM01);
    DEFINE_ABS(ITEM_HM01, ITEM_HM01);
    /* String placeholders of charmap.txt (FD 02..04), not C constants. */
    DEFINE_ABS(STR_VAR_1, 0x02FD);
    DEFINE_ABS(STR_VAR_2, 0x03FD);
    DEFINE_ABS(STR_VAR_3, 0x04FD);
}
