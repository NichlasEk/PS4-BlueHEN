/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Which Output calls the tap copies, and how many frames it lets through.
 * Header only, no dependencies, used by the plugin and by the host tests.
 *
 * Games call sceAudioOutOutput in very different ways: blocking calls at the
 * audio rate (about 47 per second), small or large buffers, and some loops
 * that call it tens of thousands of times a second and mostly get an error
 * (52,000 calls/s, 0x8026000F "not initialized").
 * A call that failed did not queue its buffer, so copying it plays old or
 * half written data. Rules, the same for every game:
 *
 *   rc >= 0            a real buffer: copied (rate limit with 2 % headroom and a
 *                      full ring of slack, so a game whose clock is a little
 *                      fast, or that fills its queue at start, loses nothing)
 *   rc < 0, the port has returned success before
 *                      not queued: skipped, never copied
 *   rc < 0, the port has never returned success
 *                      unknown: copied under a strict 48 kHz limit, but only
 *                      until the port has made 1000 calls and 2 s have passed
 *                      without one success. Then it is "dead": nothing
 *                      is copied (silence instead of crackle) until a call
 *                      returns success, which brings it back at once.
 *
 * The limiter keeps its fractional frames: a call every 19 us adds 0.9 frames
 * and the remainder stays in `frac` (dropping it would cost 5 % of the audio
 * at 50,000 calls a second). */
#ifndef HEARBRIDGE_TAP_RATE_H
#define HEARBRIDGE_TAP_RATE_H

#include <stdint.h>

#define HB_GATE_RATE_HZ      48000u
#define HB_GATE_OK_RATE_HZ   48960u    /* +2 % for calls that returned success */
#define HB_GATE_ERR_BURST    960u      /* 20 ms */
#define HB_GATE_DEAD_CALLS   1000u     /* a port that never succeeded: after this many calls ... */
#define HB_GATE_DEAD_US      2000000u  /* ... and this long, it is not tapped */

enum { HB_GATE_PASS = 1, HB_GATE_RATE_DROP = 0, HB_GATE_ERR_SKIP = -1 };

struct hb_tapgate {
    uint64_t budget_us;       /* time base of the budget (0 = not started) */
    uint32_t budget_frames;
    uint64_t frac;            /* leftover of budget_frames, in millionths of a frame */
    int ok_seen;              /* this port has returned rc >= 0 */
    uint64_t first_us;        /* first call of a port that has not succeeded yet */
    uint32_t err_n;           /* its calls so far */
    int dead;                 /* never succeeded after DEAD_CALLS and DEAD_US: not tapped */
};

static inline void hb_gate_reset(struct hb_tapgate *g)
{
    g->budget_us = 0;
    g->budget_frames = 0;
    g->frac = 0;
}

/* rc: return value of the original call. need: frames of this buffer.
 * now_us: monotonic microseconds. cap: largest budget (ring size, frames).
 * Returns HB_GATE_PASS, HB_GATE_RATE_DROP or HB_GATE_ERR_SKIP. */
static inline int hb_gate_pass(struct hb_tapgate *g, int32_t rc, uint32_t need,
                               uint64_t now_us, uint32_t cap)
{
    uint32_t rate, burst;
    uint64_t add;

    if (rc >= 0) {
        if (!g->ok_seen) {
            /* the first success: whatever the unknown phase spent is forgotten */
            g->ok_seen = 1;
            g->dead = 0;
            hb_gate_reset(g);
        }
        rate = HB_GATE_OK_RATE_HZ;
        burst = cap;                      /* starts full: a game that fills its queue at start loses nothing */
    } else if (g->ok_seen) {
        return HB_GATE_ERR_SKIP;
    } else if (g->dead) {
        return HB_GATE_ERR_SKIP;
    } else {
        if (!g->first_us) g->first_us = now_us ? now_us : 1;
        if (g->err_n < 0xFFFFFFFFu) g->err_n++;
        if (g->err_n >= HB_GATE_DEAD_CALLS && now_us - g->first_us >= HB_GATE_DEAD_US) {
            g->dead = 1;
            return HB_GATE_ERR_SKIP;
        }
        rate = HB_GATE_RATE_HZ;
        burst = HB_GATE_ERR_BURST;
    }
    if (burst < need) burst = need;           /* the first buffer always fits */
    if (g->budget_us == 0) {
        g->budget_us = now_us ? now_us : 1;
        g->budget_frames = burst;
    }
    if (now_us > g->budget_us) {
        /* exact accounting: whole frames go to the budget, the remainder
         * (millionths of a frame) is kept for the next call */
        uint64_t num = (now_us - g->budget_us) * (uint64_t)rate + g->frac;
        add = num / 1000000ull;
        g->frac = num % 1000000ull;
        g->budget_us = now_us;
        if (add > cap) {                      /* long pause: start over */
            add = cap;
            g->frac = 0;
        }
        if (add > 0) {
            if (g->budget_frames + add > cap)
                g->budget_frames = cap;
            else
                g->budget_frames += (uint32_t)add;
        }
    }
    if (g->budget_frames < need)
        return HB_GATE_RATE_DROP;
    g->budget_frames -= need;
    return HB_GATE_PASS;
}

#endif
