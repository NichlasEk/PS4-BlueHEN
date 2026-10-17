/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Diagnostics for ports whose Output calls fail. Header only, used by the
 * plugin and by the host tests. For a port that returns errors the log shows
 * which return codes it gets (distinct values and counts), whether the game
 * changes the buffer between calls (a checksum of the converted samples, taken
 * for the first 256 failing calls and then every 1024th, never on every call)
 * and the loudest sample seen, so a log tells a silent or frozen buffer from
 * real audio that the console refuses. */
#ifndef HEARBRIDGE_TAP_DIAG_H
#define HEARBRIDGE_TAP_DIAG_H

#include <stdint.h>

#define HB_DIAG_RC 4

struct hb_portdiag {
    int32_t  rc[HB_DIAG_RC];     /* the first distinct return codes */
    uint64_t rc_n[HB_DIAG_RC];   /* calls that returned each */
    uint64_t rc_other;           /* calls with a fifth or later distinct code */
    uint32_t nrc;
    uint64_t err_n;              /* failing calls seen by the sampler */
    uint32_t samples;            /* buffers checksummed */
    uint32_t changes;            /* of them, different from the one before */
    uint32_t last_hash;
    uint32_t peak;               /* loudest |sample| of all of them, 0..32767 */
};

static inline void hb_diag_rc(struct hb_portdiag *d, int32_t rc)
{
    uint32_t i;
    for (i = 0; i < d->nrc; i++)
        if (d->rc[i] == rc) { d->rc_n[i]++; return; }
    if (d->nrc < HB_DIAG_RC) {
        d->rc[d->nrc] = rc;
        d->rc_n[d->nrc] = 1;
        d->nrc++;
    } else {
        d->rc_other++;
    }
}

/* n: 1-based count of failing calls. 1 = take a checksum of this one. */
static inline int hb_diag_due(uint64_t n)
{
    return n <= 256 || (n & 1023) == 0;
}

/* FNV-1a over the samples (s16, count = samples, not frames). */
static inline uint32_t hb_diag_hash(const int16_t *pcm, uint32_t count)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < count; i++) {
        h ^= (uint16_t)pcm[i] & 0xFFu;
        h *= 16777619u;
        h ^= ((uint16_t)pcm[i] >> 8) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static inline uint32_t hb_diag_peak(const int16_t *pcm, uint32_t count)
{
    uint32_t pk = 0, i;
    for (i = 0; i < count; i++) {
        int32_t v = pcm[i];
        uint32_t a = (uint32_t)(v < 0 ? -v : v);
        if (a > pk) pk = a;
    }
    return pk > 32767u ? 32767u : pk;
}

/* One failing call: count it and, when due, checksum and measure its buffer. */
static inline void hb_diag_err(struct hb_portdiag *d, const int16_t *pcm, uint32_t count)
{
    uint32_t h, pk;
    d->err_n++;
    if (!hb_diag_due(d->err_n) || !pcm || !count) return;
    h = hb_diag_hash(pcm, count);
    pk = hb_diag_peak(pcm, count);
    if (d->samples && h != d->last_hash) d->changes++;
    d->last_hash = h;
    d->samples++;
    if (pk > d->peak) d->peak = pk;
}

#endif
