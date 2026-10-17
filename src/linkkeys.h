/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Persist Bluetooth link keys under /data/hearbridge/linkkeys.bin. */
#ifndef HEARBRIDGE_LINKKEYS_H
#define HEARBRIDGE_LINKKEYS_H

#include <stdint.h>

#define LINKKEYS_PATH     "/data/hearbridge/linkkeys.bin"
#define LINKKEYS_PATH_ALT "/user/data/hearbridge/linkkeys.bin"
#define LINKKEYS_MAX      8

/* Load a stored key for addr (HCI little-endian). Returns 1 if found. */
int linkkeys_load(const unsigned char addr[6], unsigned char key[16], int *key_type);

/* Store or update a key. Creates the directory if needed, writes a temp file,
 * fsyncs it and renames it into place. Returns 0 on success. */
int linkkeys_store(const unsigned char addr[6], const unsigned char key[16], int key_type);

/* Remove the key for addr. Returns 1 if one was removed. */
int linkkeys_delete(const unsigned char addr[6]);

/* Addresses with a stored key, oldest first. Returns count. */
int linkkeys_list(unsigned char addrs[][6], int max);

/* Path of the file written by the last successful linkkeys_store. */
const char *linkkeys_saved_path(void);

#endif
