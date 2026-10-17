/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 5-band equalizer on the PCM before SBC encoding: low shelf (bass),
 * three peaking bands, high shelf (treble). Biquads from the RBJ audio EQ
 * cookbook, float, per channel. A preamp of minus the largest boost keeps
 * headroom; the soft limiter in gain.c catches what is left.
 * Pure (no libm: the payload imports only a fixed set of libc functions), host tested.
 * Taken from HearBridge-PS5 (X-F1REBALL-X), plus hb_eq_apply for the PS4 path. */
#ifndef HEARBRIDGE_EQ_H
#define HEARBRIDGE_EQ_H

#include <stdint.h>

#define HB_EQ_NB 5

typedef struct { float b0, b1, b2, a1, a2; } hb_biquad;

typedef struct hb_eq_s {
    int on;                    /* enabled and at least one band != 0 dB */
    int db[HB_EQ_NB];
    float preamp;              /* linear, <= 1 */
    int active[HB_EQ_NB];      /* band does anything */
    hb_biquad bq[HB_EQ_NB];
    float z[HB_EQ_NB][2][2];   /* per band, per channel: transposed DF-II state */
} hb_eq;

extern const int  hb_eq_hz[HB_EQ_NB];   /* 80, 250, 1000, 3500, 10000 */

void  hb_eq_init(hb_eq *e);
/* New settings (dB per band, -12..12). Keeps the filter state, so a change
 * while streaming does not click. */
void  hb_eq_set(hb_eq *e, int on, const int db[HB_EQ_NB], int sample_rate);
/* Interleaved stereo float in place (no preamp). */
void  hb_eq_process(hb_eq *e, float *lr, int frames);
/* The PS4 audio path: int16 stereo frames, EQ (when on) then the Boost gain
 * (gain_q8, 256 = unchanged) and a soft limiter. With the EQ off this is
 * exactly hb_apply_gain (boost.h). With the EQ on the signal goes through the
 * float biquads in blocks of 256 frames; the limiter knee is 0.6 of full scale
 * when the gain is above unity (like the Boost) and 0.9 otherwise, so an
 * equalised peak is bent instead of clipped while normal levels stay as they are. */
void  hb_eq_apply(hb_eq *e, int16_t *pcm, int frames, int32_t gain_q8);
/* Response of the current settings at hz, in dB (filters only). */
float hb_eq_response_db(const hb_eq *e, float hz, int sample_rate);

/* Small math without libm (exported for the tests). */
double hb_eqm_sin(double x);
double hb_eqm_cos(double x);
double hb_eqm_pow10(double x);
double hb_eqm_sqrt(double x);
double hb_eqm_log10(double x);

#endif
