/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "devices.h"
#include "cod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int same(const unsigned char a[6], const unsigned char b[6])
{
    return memcmp(a, b, 6) == 0;
}

void devices_addr_str(const unsigned char a[6], char out[18])
{
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", a[5], a[4], a[3], a[2], a[1], a[0]);
}

static int hexv(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int devices_parse_addr(const char *s, unsigned char addr[6])
{
    int i;
    if (!s) return 0;
    for (i = 0; i < 6; i++) {
        int h = hexv(s[0]), l = hexv(s[1]);
        if (h < 0 || l < 0) return 0;
        addr[5 - i] = (unsigned char)(h * 16 + l);
        s += 2;
        if (i < 5) {
            if (*s != ':' && *s != '-') return 0;
            s++;
        }
    }
    return 1;
}

int devices_load(struct saved_dev *out)
{
    FILE *f = fopen(DEVICES_PATH, "r");
    char line[160];
    int n = 0;
    if (!f) return 0;
    while (n < DEVICES_MAX && fgets(line, sizeof line, f)) {
        char *p = line, *q;
        struct saved_dev d;
        size_t len;
        memset(&d, 0, sizeof d);
        if (!devices_parse_addr(p, d.addr)) continue;
        p = strchr(p, '|');
        if (!p) continue;
        d.cod = (uint32_t)strtoul(p + 1, &q, 16);
        p = strchr(p + 1, '|');
        if (!p) continue;
        d.seq = (uint32_t)strtoul(p + 1, &q, 10);
        p = strchr(p + 1, '|');
        if (p) {
            snprintf(d.name, sizeof d.name, "%s", p + 1);
            len = strlen(d.name);
            while (len > 0 && (d.name[len - 1] == '\n' || d.name[len - 1] == '\r'))
                d.name[--len] = 0;
        }
        out[n++] = d;
    }
    fclose(f);
    return n;
}

static int save_all(const struct saved_dev *d, int n)
{
    char tmp[96];
    FILE *f;
    int i, ok = 1;
    mkdir("/data/hearbridge", 0777);
    snprintf(tmp, sizeof tmp, "%s.tmp", DEVICES_PATH);
    f = fopen(tmp, "w");
    if (!f) return -1;
    for (i = 0; i < n; i++) {
        char a[18], name[64];
        size_t k;
        devices_addr_str(d[i].addr, a);
        snprintf(name, sizeof name, "%s", d[i].name);
        for (k = 0; name[k]; k++)
            if (name[k] == '|' || name[k] == '\n' || name[k] == '\r') name[k] = ' ';
        if (fprintf(f, "%s|%06x|%u|%s\n", a, (unsigned)d[i].cod, (unsigned)d[i].seq, name) < 0)
            ok = 0;
    }
    if (fflush(f) != 0) ok = 0;
    if (fsync((fileno)(f)) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (!ok || rename(tmp, DEVICES_PATH) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int devices_touch(const unsigned char addr[6], const char *name, uint32_t cod)
{
    struct saved_dev d[DEVICES_MAX];
    int n = devices_load(d), i, slot = -1;
    uint32_t maxseq = 0;
    for (i = 0; i < n; i++) {
        if (d[i].seq > maxseq) maxseq = d[i].seq;
        if (same(d[i].addr, addr)) slot = i;
    }
    if (slot < 0) {
        if (n >= DEVICES_MAX) {
            /* Replace the least recently used. */
            int lru = 0;
            for (i = 1; i < n; i++)
                if (d[i].seq < d[lru].seq) lru = i;
            slot = lru;
        } else {
            slot = n++;
        }
        memset(&d[slot], 0, sizeof d[slot]);
        memcpy(d[slot].addr, addr, 6);
    }
    if (name && name[0]) snprintf(d[slot].name, sizeof d[slot].name, "%s", name);
    if (cod) d[slot].cod = cod;
    d[slot].seq = maxseq + 1;
    return save_all(d, n);
}

int devices_remove(const unsigned char addr[6])
{
    struct saved_dev d[DEVICES_MAX];
    int n = devices_load(d), i, j = 0, found = 0;
    for (i = 0; i < n; i++) {
        if (same(d[i].addr, addr)) { found = 1; continue; }
        d[j++] = d[i];
    }
    if (!found) return 0;
    return save_all(d, j) == 0;
}

int devices_last(struct saved_dev *out)
{
    struct saved_dev d[DEVICES_MAX];
    int n = devices_load(d), i, best = -1;
    for (i = 0; i < n; i++)
        if (best < 0 || d[i].seq > d[best].seq) best = i;
    if (best < 0) return 0;
    *out = d[best];
    return 1;
}

/* Saved entry for addr; 1 if found (stored class in *cod). */
int devices_find(const unsigned char addr[6], uint32_t *cod)
{
    struct saved_dev d[DEVICES_MAX];
    int n = devices_load(d), i;
    for (i = 0; i < n; i++)
        if (same(d[i].addr, addr)) {
            if (cod) *cod = d[i].cod;
            return 1;
        }
    return 0;
}

/* Most recently used saved device that is an audio sink (an entry saved by
 * an older version for a TV or streamer is skipped). */
int devices_last_audio(struct saved_dev *out)
{
    struct saved_dev d[DEVICES_MAX];
    int n = devices_load(d), i, best = -1;
    for (i = 0; i < n; i++) {
        if (!cod_device_allowed(0, 1, d[i].cod)) continue;
        if (best < 0 || d[i].seq > d[best].seq) best = i;
    }
    if (best < 0) return 0;
    *out = d[best];
    return 1;
}
