/* platform_pc.h - logging and small helpers shared by the PC HAL. */
#ifndef PFR_PLATFORM_PC_H
#define PFR_PLATFORM_PC_H

/* Which game this executable is (CMake PFR_GAME_VERSION): FireRed and
 * LeafGreen are built from the same sources, one executable each. */
#if defined(LEAFGREEN)
#define PC_GAME_TITLE   "Pokemon LeafGreen"
#define PC_GAME_SHORT   "leafgreen"
#define PC_SAVE_DEFAULT "leafgreen.sav"
#else
#define PC_GAME_TITLE   "Pokemon FireRed"
#define PC_GAME_SHORT   "firered"
#define PC_SAVE_DEFAULT "firered.sav"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PC_PRINTF(f, a) __attribute__((format(printf, f, a)))
#else
#define PC_PRINTF(f, a)
#endif

void pc_log(const char *fmt, ...) PC_PRINTF(1, 2);
void pc_warn(const char *fmt, ...) PC_PRINTF(1, 2);
_Noreturn void pc_fatal(const char *fmt, ...) PC_PRINTF(1, 2);

#ifdef __cplusplus
}
#endif

#endif /* PFR_PLATFORM_PC_H */
