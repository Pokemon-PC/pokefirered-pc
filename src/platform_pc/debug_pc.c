/*
 * debug_pc.c - game-state dumps for debugging (built with the game headers).
 * Called by main_pc.c for PFR_REGS frames: main callbacks and active tasks,
 * as addresses that src/platform_pc/scripts/symbolize.py turns into names
 * (lines formatted like backtrace frames: "#NN 0x...").
 */
#include <stdlib.h>
#include <string.h>

#include "platform_pc.h"

#include "global.h"
#include "main.h"
#include "task.h"

void pc_debug_dump_game(unsigned long long frame) {
    pc_log("game@%llu state=%u", frame, gMain.state);
    pc_log("game@%llu   #00 0x%016llX callback1", frame, (unsigned long long)(uintptr_t)gMain.callback1);
    pc_log("game@%llu   #00 0x%016llX callback2", frame, (unsigned long long)(uintptr_t)gMain.callback2);
    pc_log("game@%llu   #00 0x%016llX vblankCallback", frame, (unsigned long long)(uintptr_t)gMain.vblankCallback);
    for (int i = 0; i < NUM_TASKS; i++) {
        if (!gTasks[i].isActive) {
            continue;
        }
        pc_log("game@%llu   #00 0x%016llX task %d data=%d,%d,%d,%d,%d,%d,%d,%d", frame,
               (unsigned long long)(uintptr_t)gTasks[i].func, i, gTasks[i].data[0], gTasks[i].data[1],
               gTasks[i].data[2], gTasks[i].data[3], gTasks[i].data[4], gTasks[i].data[5],
               gTasks[i].data[6], gTasks[i].data[7]);
    }
}

/* ---- test hooks (PFR_TEST) --------------------------------------------------
 * PFR_TEST="battle@N": at the first frame >= N where the player is free to
 * move in the overworld, give a level-5 Charmander (if the party is empty) and
 * start a wild battle against a level-3 Rattata through the game's own
 * StartWildBattle. Exercises the whole battle engine. */
#include "battle_setup.h"
#include "event_data.h"
#include "overworld.h"
#include "pokemon.h"
#include "script.h"
#include "script_pokemon_util.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/species.h"

static bool8 player_free_in_overworld(void) {
    return gMain.callback1 == CB1_Overworld && !ArePlayerFieldControlsLocked() && !ScriptContext_IsEnabled();
}

/* PFR_TRACE=1: one log line whenever the map, the player's position or
 * "player has control" changes - anchors for writing PFR_INPUT scripts. */
static void trace(unsigned long long frame) {
    static int last_group = -1, last_num = -1, last_x = -9999, last_y = -9999, last_free = -1;
    static void (*last_cb2)(void);
    if (gSaveBlock1Ptr == NULL) {
        return;
    }
    int group = gSaveBlock1Ptr->location.mapGroup, num = gSaveBlock1Ptr->location.mapNum;
    int x = gSaveBlock1Ptr->pos.x, y = gSaveBlock1Ptr->pos.y;
    int free = player_free_in_overworld();
    if (group != last_group || num != last_num || x != last_x || y != last_y || free != last_free
        || gMain.callback2 != last_cb2) {
        pc_log("trace@%llu map=%d.%d pos=%d,%d free=%d   #00 0x%016llX callback2", frame, group, num, x, y, free,
               (unsigned long long)(uintptr_t)gMain.callback2);
        last_group = group; last_num = num; last_x = x; last_y = y; last_free = free;
        last_cb2 = gMain.callback2;
    }
}

void pc_debug_frame(unsigned long long frame) {
    static int tracing = -1;
    if (tracing < 0) {
        const char *t = getenv("PFR_TRACE");
        tracing = t && *t && *t != '0';
    }
    if (tracing) {
        trace(frame);
    }
    /* PFR_TEST="givemon@N,battle@N": actions run at the first frame >= N
     * where the player is free in the overworld, each once. */
    static char *spec;
    static bool8 parsed;
    static unsigned long long give_at = ~0ull, battle_at = ~0ull;
    if (!parsed) {
        parsed = TRUE;
        const char *e = getenv("PFR_TEST");
        if (e && *e) {
            spec = strdup(e);
            for (char *tok = strtok(spec, ","); tok; tok = strtok(NULL, ",")) {
                if (strncmp(tok, "givemon@", 8) == 0) give_at = strtoull(tok + 8, NULL, 10);
                else if (strncmp(tok, "battle@", 7) == 0) battle_at = strtoull(tok + 7, NULL, 10);
                else pc_warn("PFR_TEST: unknown action '%s'", tok);
            }
        }
    }
    bool8 want_give = frame >= give_at, want_battle = frame >= battle_at;
    if ((!want_give && !want_battle) || !player_free_in_overworld()) {
        return;
    }
    if (GetMonData(&gPlayerParty[0], MON_DATA_SPECIES) == SPECIES_NONE) {
        ScriptGiveMon(SPECIES_CHARMANDER, 5, ITEM_NONE, 0, 0, 0);
        FlagSet(FLAG_SYS_POKEMON_GET);
        pc_log("test: gave a level-5 Charmander at frame %llu", frame);
    }
    if (want_give) {
        give_at = ~0ull;
    }
    if (want_battle) {
        battle_at = ~0ull;
        CreateMon(&gEnemyParty[0], SPECIES_RATTATA, 3, USE_RANDOM_IVS, 0, 0, OT_ID_PLAYER_ID, 0);
        pc_log("test: wild battle (Charmander L5 vs Rattata L3) at frame %llu", frame);
        StartWildBattle();
    }
}
