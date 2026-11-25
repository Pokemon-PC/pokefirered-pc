/*
 * pc_hal.h - what the game (under PLATFORM_PC) calls into the PC HAL.
 *
 * Only plain C types here: this header is included from the game's own
 * headers (include/gba/macro.h) and from HAL files that do not see them.
 */
#ifndef PFR_PC_HAL_H
#define PFR_PC_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- DMA (dma_pc.c) -------------------------------------------------------
 * The DmaSet/DmaStop macros call these instead of writing REG_DMAxSAD/DAD/CNT:
 * a store to plain memory cannot start a transfer on PC. Full 64-bit
 * pointers are kept; the registers still receive their (32-bit) values for
 * code that reads them back. control = DMAxCNT_H << 16 | count. */
void pc_dma_set(int ch, const volatile void *src, volatile void *dest, uint32_t control);
void pc_dma_stop(int ch);
void pc_dma_on_hblank(void);   /* visible lines only */
void pc_dma_on_vblank(void);

/* ---- interrupts and frame timing (irq_pc.c) ------------------------------ */
typedef void (*PcIntrFunc)(void);
/* Table in the game's order (gIntrTableTemplate: VCount, Serial, Timer3,
 * HBlank, VBlank, Timer0-2, DMA0-3, Keypad, GamePak), as IntrMain uses it. */
void pc_irq_set_table(PcIntrFunc *table, int count);
/* Raises an IF flag (INTR_FLAG_*) and runs the handler if IME/IE allow it. */
void pc_irq_raise(uint16_t flag);
/* Runs the display until the start of the next VBlank: renders the
 * remaining lines (VCount/HBlank IRQs, HBlank DMA), presents the frame,
 * paces to 59.73 Hz, reads input, then raises the VBlank IRQ.
 * This is what WaitForVBlank()/VBlankIntrWait() do on PC. */
void pc_wait_vblank(void);
uint64_t pc_frame_count(void);
/* One scanline of time passes (rendering, IRQs, DMA). For game loops that
 * busy-wait on a register the hardware changes by itself (REG_VCOUNT...). */
void pc_advance_line(void);
/* VCOUNT as seen by code deciding "is it safe to touch the display now":
 * the real line inside IRQ handlers run by the line loop, otherwise 161
 * (the main loop runs between frames on PC). */
uint16_t pc_vcount_for_game_code(void);

/* ---- timers (timer_pc.c) -------------------------------------------------- */
void pc_timers_step(uint32_t cycles);
void pc_timers_reset(void);

/* ---- video and input (video_pc.c) ---------------------------------------- */
bool pc_video_init(void);          /* PFR_HEADLESS=1: no window */
void pc_video_present(const uint32_t *argb240x160);
/* Polls host events; returns the GBA keys held (active high, KEY_* bits). */
uint16_t pc_input_poll(void);
bool pc_quit_requested(void);
void pc_video_shutdown(void);
/* Test hook: when set, replaces keyboard/gamepad input for every frame. */
void pc_input_set_override(uint16_t (*fn)(uint64_t frame));
/* Test hook: called with every finished frame (240x160 ARGB). */
void pc_video_set_frame_hook(void (*fn)(uint64_t frame, const uint32_t *argb));

/* ---- backup memory (flash_pc.c) ------------------------------------------ */
/* Path of the 128 KiB flash image; default "save.bin" (env PFR_SAVE). */
void pc_flash_set_path(const char *path);
const uint8_t *pc_flash_data(void);
/* Writes pending changes to disk (called at each VBlank and at exit). */
void pc_flash_flush(void);

/* ---- config.ini (config_pc.c) -------------------------------------------- */
const char *pc_config_get(const char *section, const char *key, const char *def);
int pc_config_int(const char *section, const char *key, int def);

/* ---- sound output (audio_pc.c) -------------------------------------------- */
void pc_audio_init(void);   /* PFR_MUTE=1: no device; PFR_WAV=file.wav: record */
/* One VBlank of DirectSound (FIFO A = right, FIFO B = left, n samples at the
 * engine rate) plus the PSG channels read from NR10-NR52. */
void pc_audio_frame(const int8_t *right, const int8_t *left, int n);
void pc_audio_shutdown(void);

/* ---- crash reports and hang watchdog (crash_pc.c) ------------------------ */
void pc_crash_install(void);   /* call on the game thread */
void pc_watchdog_kick(void);   /* once per frame */
void pc_log_backtrace(const char *tag);
/* nullread_pc.c: emulates a faulting access to the GBA BIOS range
 * and unused area (0..0x1FFFFFF, i.e. through a NULL-based pointer) as on the GBA: reads give 0,
 * writes are dropped. regs[] in x86 encoding order (rax, rcx, rdx, rbx, rsp,
 * rbp, rsi, rdi, r8..r15). */
bool pc_emulate_null_access(uint64_t *regs[16], uint64_t *rip, uint32_t *eflags, uintptr_t fault, bool write);

#ifdef __cplusplus
}
#endif

#endif /* PFR_PC_HAL_H */
