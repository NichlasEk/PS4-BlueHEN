/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Saved audio devices (headphones, earbuds, speakers) for HearBridge PS4.
 * /data/hearbridge/devices.txt, one per line:
 *   AA:BB:CC:DD:EE:FF|cod_hex|last_used_seq|name
 * The device with the highest last_used_seq is the "last used" one. */
#ifndef HEARBRIDGE_DEVICES_H
#define HEARBRIDGE_DEVICES_H

#include <stdint.h>

#define DEVICES_MAX  8
#define DEVICES_PATH "/data/hearbridge/devices.txt"

struct saved_dev {
    unsigned char addr[6];      /* HCI little-endian */
    uint32_t cod;
    uint32_t seq;               /* higher = used more recently */
    char name[64];
};

/* Load into out[DEVICES_MAX]; returns count (0 if none). */
int devices_load(struct saved_dev *out);

/* Add or update (name/cod if non-empty/non-zero) and mark as last used. */
int devices_touch(const unsigned char addr[6], const char *name, uint32_t cod);

/* Remove; returns 1 if removed. */
int devices_remove(const unsigned char addr[6]);

/* Last used device; returns 1 and fills *out if any. */
int devices_last(struct saved_dev *out);

/* Saved entry for addr: 1 and its stored class in *cod. */
int devices_find(const unsigned char addr[6], uint32_t *cod);

/* Last used saved device that passes the audio class filter (cod.h). */
int devices_last_audio(struct saved_dev *out);

/* "AA:BB:... (display order, big-endian) <-> HCI little-endian. */
void devices_addr_str(const unsigned char addr[6], char out[18]);
int  devices_parse_addr(const char *s, unsigned char addr[6]);

#endif
