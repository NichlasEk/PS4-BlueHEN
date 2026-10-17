/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Replace a file in one step: write path.tmp, flush it to disk, then rename it
 * over the real file, so a crash or power loss leaves either the old content
 * or the new one, never half of it. Header only, host tests include it. */
#ifndef HEARBRIDGE_ATOMICFILE_H
#define HEARBRIDGE_ATOMICFILE_H

#include <stdio.h>
#include <unistd.h>

/* 0 on success, -1 when anything failed (the old file is then untouched). */
static inline int hb_file_write_atomic(const char *path, const void *data, size_t n)
{
    char tmp[160];
    FILE *f;
    int ok = 1;
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return -1;
    if (!(f = fopen(tmp, "wb"))) return -1;
    if (n > 0 && fwrite(data, 1, n, f) != n) ok = 0;
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

#endif
