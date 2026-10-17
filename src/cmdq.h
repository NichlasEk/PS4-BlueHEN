/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Small queue for the commands the web page sends to the daemon, so two quick
 * clicks (Disconnect then Connect, two Forgets) are not lost the way a single
 * slot lost them. Header only, no locking here: the caller holds the UI lock.
 * When full, the oldest entry is dropped so the newest click still counts. */
#ifndef HEARBRIDGE_CMDQ_H
#define HEARBRIDGE_CMDQ_H

#include <string.h>

#define CMDQ_N 8

struct cmdq_item {
    int type;
    unsigned char addr[6];
};

struct cmdq {
    struct cmdq_item q[CMDQ_N];
    unsigned head, count;
};

static inline void cmdq_push(struct cmdq *c, int type, const unsigned char *addr)
{
    struct cmdq_item *it;
    unsigned last;
    /* The same command twice in a row is one command. */
    if (c->count) {
        last = (c->head + c->count - 1) % CMDQ_N;
        if (c->q[last].type == type &&
            (!addr || memcmp(c->q[last].addr, addr, 6) == 0))
            return;
    }
    if (c->count == CMDQ_N) {         /* drop the oldest */
        c->head = (c->head + 1) % CMDQ_N;
        c->count--;
    }
    it = &c->q[(c->head + c->count) % CMDQ_N];
    it->type = type;
    memset(it->addr, 0, 6);
    if (addr) memcpy(it->addr, addr, 6);
    c->count++;
}

/* 1 and the oldest entry, or 0 when empty. */
static inline int cmdq_pop(struct cmdq *c, int *type, unsigned char addr[6])
{
    struct cmdq_item *it;
    if (!c->count) return 0;
    it = &c->q[c->head];
    *type = it->type;
    memcpy(addr, it->addr, 6);
    c->head = (c->head + 1) % CMDQ_N;
    c->count--;
    return 1;
}

#endif
