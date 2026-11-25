/*
 * isagbprn_pc.c - replaces src/isagbprn.c (AGBPrint: the debug unit's
 * printf, ARM code writing to a port in the cartridge area).
 *
 * FireRed/LeafGreen shipped without NDEBUG (include/config.h): the retail
 * ROM keeps its AGB_ASSERT checks, which print through AGBPrint. On a retail
 * GBA the prints go nowhere; on PC they go to the log, once per message.
 * A failing assert "with stop" executes an undefined SWI on the GBA; here it
 * is logged with a backtrace and the game goes on.
 */
/* The game keeps its asserts (no NDEBUG, as pp_c.py preprocesses it). */
#undef NDEBUG
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pc_hal.h"
#include "platform_pc.h"

#include "global.h"

void AGBPrintInit(void) {}

static char s_line[256];
static size_t s_len;

static void flush_line(void) {
    if (s_len) {
        s_line[s_len] = '\0';
        pc_log("agbprint: %s", s_line);
        s_len = 0;
    }
}

void AGBPutc(const char c) {
    if (c == '\n' || s_len + 1 >= sizeof s_line) {
        flush_line();
        if (c == '\n') {
            return;
        }
    }
    s_line[s_len++] = c;
}

void AGBPrint(const char *buf) {
    while (*buf) {
        AGBPutc(*buf++);
    }
}

void AGBPrintf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    AGBPrint(buf);
}

void AGBPrintFlush1Block(void) {
    flush_line();
}

void AGBPrintFlush(void) {
    flush_line();
}

void AGBAssert(const char *file, int line, const char *expr, int stop) {
    pc_warn("game %s: %s:%d: %s", stop ? "assertion failed" : "warning", file, line, expr);
    if (stop) {
        pc_log_backtrace("[pfr:assert]");
    }
}
