/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Linear-interpolation resampler core shared by the PCM ring (pcm_udp.c)
 * and the host tests. Positions are 16.16 fixed point, in input frames. */
#ifndef HEARBRIDGE_RESAMPLE_H
#define HEARBRIDGE_RESAMPLE_H

#include <stdint.h>

/* a + (b - a) * f / 65536 for f in 0..65535. The product of two 16-bit
 * quantities and f does not fit in 32 bits (that was the click in loud
 * passages), so it is done in 64 bits. */
static inline int16_t hb_lerp16(int32_t a, int32_t b, uint32_t f)
{
    return (int16_t)(a + (int32_t)(((int64_t)(b - a) * (int64_t)f) >> 16));
}

/* Input frames per output frame, 16.16: 48000 / out_hz, corrected by a
 * drift of ppm parts per million (positive = read faster). */
static inline uint32_t hb_rs_step(int out_hz, double ppm)
{
    double r = 48000.0 / (double)(out_hz > 0 ? out_hz : 48000);
    return (uint32_t)(65536.0 * r * (1.0 + ppm * 1e-6) + 0.5);
}

/* Interpolate up to max_out stereo frames. at(ctx, frame, ch) reads input
 * frame `frame` (0..fill-1). Stops when the next output would need a frame
 * at or past fill. *pos starts at the phase and ends advanced by step per
 * output (its integer part is the number of input frames consumed). */
typedef int16_t (*hb_rs_at_fn)(void *ctx, int frame, int ch);

static inline int hb_rs_run(hb_rs_at_fn at, void *ctx, int fill, uint32_t *pos,
                            uint32_t step, int16_t *out, int max_out)
{
    int got = 0;
    uint32_t p = *pos;
    while (got < max_out) {
        int k = (int)(p >> 16);
        uint32_t f = p & 0xFFFFu;
        int ch;
        if (k + 1 >= fill) break;
        for (ch = 0; ch < 2; ch++)
            out[got * 2 + ch] = hb_lerp16(at(ctx, k, ch), at(ctx, k + 1, ch), f);
        got++;
        p += step;
    }
    *pos = p;
    return got;
}

#endif
