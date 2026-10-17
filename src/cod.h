/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Class of Device filter: only headphones, headsets and speakers are
 * listed, paired or connected. Header only, no dependencies (host tests
 * include it directly). */
#ifndef HEARBRIDGE_COD_H
#define HEARBRIDGE_COD_H

#include <stdint.h>

#define COD_MAJOR(c)   ((int)(((c) >> 8) & 0x1F))
#define COD_MINOR(c)   ((int)(((c) >> 2) & 0x3F))

/* Service class bits that audio sinks do not set but TVs and streamers do. */
#define COD_SVC_CAPTURING  0x080000u
#define COD_SVC_OBJECT_TX  0x100000u

/* 1 if the class is an audio sink we want: major Audio/Video (0x04) with
 * minor wearable headset 0x01, hands-free 0x02, loudspeaker 0x05,
 * headphones 0x06, portable audio 0x07, car audio 0x08 or hi-fi 0x0A.
 * Everything else (set-top box 0x09, video display 0x0F, monitors,
 * cameras, phones, computers, controllers, keyboards, unknown 0) is out.
 * TVs and streamers often claim car audio (TCL TV 0x2c0420, HOT Streamer
 * 0x3c0420), but they also set the Capturing / Object Transfer service
 * bits, so car audio counts only without those two bits. */
static inline int cod_is_audio_sink(uint32_t cod)
{
    if (COD_MAJOR(cod) != 0x04) return 0;
    switch (COD_MINOR(cod)) {
    case 0x01: case 0x02: case 0x05: case 0x06: case 0x07: case 0x0A:
        return 1;
    case 0x08:
        return (cod & (COD_SVC_CAPTURING | COD_SVC_OBJECT_TX)) == 0;
    default:
        return 0;
    }
}

/* Decision for a device we may talk to (scan result, connect command,
 * incoming connection).
 *   cod        class seen now (0 = not known in this event)
 *   saved      1 if the address is in the saved list
 *   saved_cod  class stored with the saved entry (0 = none)
 * A known class decides. An unknown class is accepted only for a saved
 * device whose stored class is audio or was never recorded, so saved
 * headphones keep working when a later event carries no class. */
static inline int cod_device_allowed(uint32_t cod, int saved, uint32_t saved_cod)
{
    if (cod) return cod_is_audio_sink(cod);
    if (!saved) return 0;
    return saved_cod == 0 || cod_is_audio_sink(saved_cod);
}

#endif
