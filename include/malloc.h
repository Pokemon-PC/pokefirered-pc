#ifndef GUARD_MALLOC_H
#define GUARD_MALLOC_H

#include "global.h"

#ifdef PLATFORM_PC
// The heap lives in the GBA's EWRAM (0x02000000, reserved by the PC HAL), as
// on hardware where the linker script puts gHeap first in EWRAM: code checks
// heap pointers against the memory map (bg.c). Four times larger: structures
// that hold pointers grow on a 64-bit host.
#define HEAP_SIZE (0x1C000 * 4)
#else
#define HEAP_SIZE 0x1C000
#endif
#define malloc Alloc
#define calloc(ct, sz) AllocZeroed((ct) * (sz))
#define free Free

#define FREE_AND_SET_NULL(ptr)          \
{                                       \
    free(ptr);                          \
    ptr = NULL;                         \
}

#define TRY_FREE_AND_SET_NULL(ptr) if (ptr != NULL) FREE_AND_SET_NULL(ptr)

#ifdef PLATFORM_PC
#define gHeap ((u8 *)EWRAM_START)
#else
extern u8 gHeap[];
#endif
void *Alloc(u32 size);
void *AllocZeroed(u32 size);
void Free(void *pointer);
void InitHeap(void *pointer, u32 size);

#endif // GUARD_MALLOC_H
