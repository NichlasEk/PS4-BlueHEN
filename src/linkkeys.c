/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "linkkeys.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC 0x48424C4Bu /* 'HBLK' */
#define VER   1u

struct entry {
    unsigned char addr[6];
    unsigned char key_type;
    unsigned char pad;
    unsigned char key[16];
};

struct file_hdr {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
};

static int same_addr(const unsigned char a[6], const unsigned char b[6])
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] &&
           a[3] == b[3] && a[4] == b[4] && a[5] == b[5];
}

static FILE *open_rw(const char *path, const char *mode)
{
    return fopen(path, mode);
}

static void ensure_dir(void)
{
    mkdir("/data/hearbridge", 0777);
    mkdir("/user/data/hearbridge", 0777);
}

static int read_all(struct entry *out, int *count)
{
    FILE *f;
    struct file_hdr h;
    size_t n;

    *count = 0;
    f = open_rw(LINKKEYS_PATH, "rb");
    if (!f) f = open_rw(LINKKEYS_PATH_ALT, "rb");
    if (!f) return -1;
    if (fread(&h, 1, sizeof h, f) != sizeof h || h.magic != MAGIC || h.version != VER) {
        fclose(f);
        return -1;
    }
    if (h.count > LINKKEYS_MAX) h.count = LINKKEYS_MAX;
    n = fread(out, sizeof(struct entry), h.count, f);
    fclose(f);
    *count = (int)n;
    return 0;
}

static const char *g_saved_path = LINKKEYS_PATH;

/* Write to a temp file, flush it to disk, then rename over the real file so
 * a crash or power loss never leaves a half-written key store. */
static int write_one(const char *path, const struct entry *ents, int count)
{
    char tmp[96];
    FILE *f;
    struct file_hdr h;
    int ok = 1;

    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    f = open_rw(tmp, "wb");
    if (!f) return -1;
    h.magic = MAGIC;
    h.version = VER;
    h.count = (uint32_t)count;
    if (fwrite(&h, 1, sizeof h, f) != sizeof h) ok = 0;
    if (ok && count > 0 &&
        fwrite(ents, sizeof(struct entry), (size_t)count, f) != (size_t)count)
        ok = 0;
    if (ok && fflush(f) != 0) ok = 0;
    /* (fileno) calls the function: the stdio macro would read the
     * __isthreaded data symbol, which payload stubs do not provide. */
    if (ok && fsync((fileno)(f)) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok || rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

static int write_all(const struct entry *ents, int count)
{
    ensure_dir();
    if (write_one(LINKKEYS_PATH, ents, count) == 0) {
        g_saved_path = LINKKEYS_PATH;
        return 0;
    }
    if (write_one(LINKKEYS_PATH_ALT, ents, count) == 0) {
        g_saved_path = LINKKEYS_PATH_ALT;
        return 0;
    }
    return -1;
}

const char *linkkeys_saved_path(void)
{
    return g_saved_path;
}

int linkkeys_load(const unsigned char addr[6], unsigned char key[16], int *key_type)
{
    struct entry ents[LINKKEYS_MAX];
    int n = 0, i;

    if (read_all(ents, &n) != 0) return 0;
    for (i = 0; i < n; i++) {
        if (same_addr(ents[i].addr, addr)) {
            memcpy(key, ents[i].key, 16);
            if (key_type) *key_type = ents[i].key_type;
            return 1;
        }
    }
    return 0;
}

int linkkeys_store(const unsigned char addr[6], const unsigned char key[16], int key_type)
{
    struct entry ents[LINKKEYS_MAX];
    int n = 0, i, slot = -1;

    (void)read_all(ents, &n);
    for (i = 0; i < n; i++) {
        if (same_addr(ents[i].addr, addr)) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (n >= LINKKEYS_MAX) {
            /* Drop the oldest entry. */
            memmove(&ents[0], &ents[1], sizeof(ents[0]) * (LINKKEYS_MAX - 1));
            n = LINKKEYS_MAX - 1;
        }
        slot = n++;
    }
    memcpy(ents[slot].addr, addr, 6);
    ents[slot].key_type = (unsigned char)key_type;
    ents[slot].pad = 0;
    memcpy(ents[slot].key, key, 16);
    return write_all(ents, n);
}

int linkkeys_delete(const unsigned char addr[6])
{
    struct entry ents[LINKKEYS_MAX];
    int n = 0, i, j = 0, found = 0;

    if (read_all(ents, &n) != 0) return 0;
    for (i = 0; i < n; i++) {
        if (same_addr(ents[i].addr, addr)) { found = 1; continue; }
        ents[j++] = ents[i];
    }
    if (!found) return 0;
    return write_all(ents, j) == 0;
}

int linkkeys_list(unsigned char addrs[][6], int max)
{
    struct entry ents[LINKKEYS_MAX];
    int n = 0, i;
    if (read_all(ents, &n) != 0) return 0;
    if (n > max) n = max;
    for (i = 0; i < n; i++) memcpy(addrs[i], ents[i].addr, 6);
    return n;
}
