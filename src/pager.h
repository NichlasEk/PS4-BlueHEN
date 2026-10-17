/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Paging budget. Outgoing pages happen only when the user asks or the daemon
 * starts:
 *   daemon start                         HB_PAGE_TRIES_START page  (about 5 s)
 *   Connect / Reconnect / Try now, pairing try   HB_PAGE_TRIES pages (about 25 s)
 * Never after a stream ended or a link dropped (the headphones are in their
 * case, they call us when it opens), never after a scan, never in the WAITING
 * state. Each page is followed by a short listening gap, the last one by
 * silence: once the budget is used up nothing is paged until the next
 * trigger. Incoming calls from saved headphones are accepted in every phase
 * (that is not paging and not part of this budget). Header only, host tests
 * include it. */
#ifndef HEARBRIDGE_PAGER_H
#define HEARBRIDGE_PAGER_H

#define HB_PAGE_TRIES 4
#define HB_PAGE_TRIES_START 1   /* daemon start: one page, 5.12 s for a saved headset */

struct hb_pager { int budget; };

static inline void hb_pager_trigger(struct hb_pager *p) { p->budget = HB_PAGE_TRIES; }
static inline void hb_pager_quiet(struct hb_pager *p) { p->budget = 0; }

/* 1 when a page may be sent now (one is used up), 0 in the quiet phase. */
static inline int hb_pager_take(struct hb_pager *p)
{
    if (p->budget <= 0) return 0;
    p->budget--;
    return 1;
}

/* A stream ended or the link dropped, whatever the reason (case closed, power
 * off, out of range): no paging at all. Returns the pages given (0). */
static inline int hb_pager_after_stream(struct hb_pager *p, int reason)
{
    (void)reason;
    hb_pager_quiet(p);
    return 0;
}

/* Listening gap after the 1st, 2nd, ... failed page, in ms. */
static inline int hb_pager_gap_ms(int attempt)
{
    static const int gap[] = { 1000, 2000, 2000, 3000 };
    return gap[attempt < 0 ? 0 : attempt > 3 ? 3 : attempt];
}

/* Longest time a window can take: every page times out, every gap but the
 * last is listened through. */
static inline long hb_pager_pages_ms(int pages, long page_timeout_ms)
{
    long t = pages * page_timeout_ms;
    int i;
    for (i = 0; i < pages - 1; i++) t += hb_pager_gap_ms(i);
    return t;
}
static inline long hb_pager_window_ms(long page_timeout_ms)
{
    return hb_pager_pages_ms(HB_PAGE_TRIES, page_timeout_ms);
}

#endif
