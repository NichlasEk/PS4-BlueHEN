/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Sample format conversion for the tap: any sceAudioOutOpen format into s16
 * stereo at the same rate. Header only (no SDK), the plugin and the host tests
 * include it. Mono is duplicated, 8 channels (7.1 or 5.1 in an 8 channel
 * frame) are folded down, float is limited softly and NaN becomes silence. */
#ifndef HEARBRIDGE_TAP_CONVERT_H
#define HEARBRIDGE_TAP_CONVERT_H

#include <stdint.h>
#include <string.h>

/* sceAudioOutOpen data formats (param & 0xff). */
#define FMT_S16_MONO      0
#define FMT_S16_STEREO    1
#define FMT_S16_8CH       2
#define FMT_FLOAT_MONO    3
#define FMT_FLOAT_STEREO  4
#define FMT_FLOAT_8CH     5
#define FMT_S16_8CH_STD   6
#define FMT_FLOAT_8CH_STD 7

static inline int16_t clamp16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static inline int16_t f2s(float v)
{
    if (v != v) return 0; /* NaN */
    if (v >= 1.0f) return 32767;
    if (v <= -1.0f) return -32768;
    return (int16_t)(v * 32767.0f);
}

/* 7.1 / 5.1 to stereo (ITU-R BS.775 style). Channel order:
 *   FMT_*_8CH     (0x2/0x5): L R C LFE Ls Rs Lb Rb
 *   FMT_*_8CH_STD (0x6/0x7): L R C LFE Lb Rb Ls Rs
 * In both, 0/1 are front, 2 center, 3 LFE, and 4..7 are two pairs with left
 * on even and right on odd, so one formula serves both:
 *   L = FL + 0.707 C + 0.707 (pairL1 + pairL2) + 0.25 LFE  (R likewise)
 * No global attenuation (it makes game audio quiet); peaks are handled
 * by a soft limiter instead. */
#define DM_C    0.70710678f
#define DM_S    0.70710678f
#define DM_LFE  0.25f
#define SC_KNEE 0.85f

/* Soft limiter: linear below the knee, smoothly approaches 1.0 above. */
static inline float softclip(float x)
{
    float a = x < 0 ? -x : x;
    float e, y;
    if (a <= SC_KNEE)
        return x;
    e = a - SC_KNEE;
    y = SC_KNEE + e / (1.0f + e / (1.0f - SC_KNEE));
    return x < 0 ? -y : y;
}

/* Convert n frames of format fmt from src into dst (s16 stereo, n frames).
 * Returns n, or 0 for a format that is not known. */
static uint32_t hb_convert(uint32_t fmt, uint32_t n, const void *src, int16_t *dst)
{
    uint32_t i;

    switch (fmt) {
    case FMT_S16_STEREO:
        memcpy(dst, src, (size_t)n * 4u);
        break;
    case FMT_S16_MONO: {
        const int16_t *s = (const int16_t *)src;
        for (i = 0; i < n; i++) {
            dst[i * 2] = s[i];
            dst[i * 2 + 1] = s[i];
        }
        break;
    }
    case FMT_FLOAT_STEREO: {
        const float *f = (const float *)src;
        for (i = 0; i < n * 2; i++)
            dst[i] = f2s(softclip(f[i]));
        break;
    }
    case FMT_FLOAT_MONO: {
        const float *f = (const float *)src;
        for (i = 0; i < n; i++) {
            int16_t v = f2s(f[i]);
            dst[i * 2] = v;
            dst[i * 2 + 1] = v;
        }
        break;
    }
    case FMT_S16_8CH:
    case FMT_S16_8CH_STD: {
        const int16_t *s = (const int16_t *)src;
        const float k = 1.0f / 32768.0f;
        for (i = 0; i < n; i++) {
            const int16_t *c = s + i * 8;
            float lfe = DM_LFE * c[3];
            float l = c[0] + DM_C * c[2] + DM_S * (c[4] + c[6]) + lfe;
            float r = c[1] + DM_C * c[2] + DM_S * (c[5] + c[7]) + lfe;
            dst[i * 2] = f2s(softclip(l * k));
            dst[i * 2 + 1] = f2s(softclip(r * k));
        }
        break;
    }
    case FMT_FLOAT_8CH:
    case FMT_FLOAT_8CH_STD: {
        const float *f = (const float *)src;
        for (i = 0; i < n; i++) {
            const float *c = f + i * 8;
            float lfe = DM_LFE * c[3];
            float l = c[0] + DM_C * c[2] + DM_S * (c[4] + c[6]) + lfe;
            float r = c[1] + DM_C * c[2] + DM_S * (c[5] + c[7]) + lfe;
            dst[i * 2] = f2s(softclip(l));
            dst[i * 2 + 1] = f2s(softclip(r));
        }
        break;
    }
    default:
        return 0;
    }
    return n;
}


#endif
