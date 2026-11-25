/*
 * layout_check_pc.c - structs shared with the assembly data must keep their
 * GBA layout on PC (PC_PTR32 pointer fields): the .s/.inc files and mapjson /
 * mid2agb output lay them out with 4-byte pointers. A failure here means a
 * pointer field lost its PC_PTR32 annotation.
 */
#include <stddef.h>

#include "global.h"
#include "gba/m4a_internal.h"

#define SIZE(T, n) _Static_assert(sizeof(T) == (n), #T " must keep its GBA size " #n)
#define AT(T, f, n) _Static_assert(offsetof(T, f) == (n), #T "." #f " must stay at " #n)

/* data/maps, data/layouts (mapjson) */
SIZE(struct MapLayout, 0x1C);   /* + borderWidth, borderHeight (FireRed) */
SIZE(struct ObjectEventTemplate, 0x18);
AT(struct ObjectEventTemplate, script, 0x10);
SIZE(struct WarpEvent, 0x08);
SIZE(struct CoordEvent, 0x10);
AT(struct CoordEvent, script, 0x0C);
SIZE(struct BgEvent, 0x0C);
SIZE(struct MapEvents, 0x14);
SIZE(struct MapConnection, 0x0C);
SIZE(struct MapConnections, 0x08);
SIZE(struct MapHeader, 0x1C);
AT(struct MapHeader, music, 0x10);

/* sound (sound/ .inc files, mid2agb songs, gMPlayTrack_* = TRACK_SIZE 0x50 each) */
SIZE(struct ToneData, 0x0C);
SIZE(struct SongHeader, 0x0C);
SIZE(struct Song, 0x08);
SIZE(struct MusicPlayer, 0x0C);
SIZE(struct MusicPlayerTrack, 0x50);

/* SaveBlock1 embeds ObjectEventTemplates: the save format stays the GBA's. */

/* librfu: the PC layout of the API buffer (rfu_initializeAPI) must fit in
 * gRfuAPIBuffer (0x1800 bytes on PC). */
#include "librfu.h"
#define A8(n) (((n) + 7) & ~7)
_Static_assert(A8(sizeof(struct RfuLinkStatus)) + A8(sizeof(struct RfuStatic)) + A8(sizeof(struct RfuFixed))
               + RFU_CHILD_MAX * (sizeof(struct RfuSlotStatusNI) + sizeof(struct RfuSlotStatusUNI))
               + sizeof(struct RfuIntrStruct) <= 0x1800, "gRfuAPIBuffer too small for the PC layout");

/* The game heap is placed at EWRAM_START (include/malloc.h): it must fit in
 * the EWRAM area the HAL reserves. */
#include "hw_pc.h"
#include "malloc.h"
_Static_assert(HEAP_SIZE <= PC_EWRAM_SIZE, "game heap larger than the reserved EWRAM");
