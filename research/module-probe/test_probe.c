/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include "probe.c"
static int loads, unloads, lookups, fail_load = -1, fail_unload = -1;
static uint32_t fw = 0x09600011;
int32_t sceKernelGetSystemSwVersion(struct sw_version *v) { v->version = fw; return 0; }
int32_t sceKernelLoadStartModule(const char *p, size_t s, const void *a, uint32_t f, void *o, int32_t *r)
{
    assert(p == modules[loads] && !s && !a && !f && !o);
    *r = 0;
    int i = loads++;
    return i == fail_load ? -1 : 100 + i;
}
int32_t sceKernelStopUnloadModule(int32_t h, size_t s, const void *a, uint32_t f, void *o, int32_t *r)
{
    assert(!s && !a && !f && !o);
    int acquired = fail_load < 0 ? 4 : fail_load;
    assert(h == 100 + acquired - 1 - unloads);
    *r = 0;
    return unloads++ == fail_unload ? -1 : 0;
}
int32_t sceKernelDlsym(int32_t h, const char *n, void **p)
{
    assert(h == 103 && n == symbols[lookups++]);
    *p = (void *)(uintptr_t)1; /* Invalid callable address: must never execute. */
    return 0;
}
int main(void)
{
    FILE *log = tmpfile(); assert(log);
    fw = 0x11000000;
    assert(bluehen_modules(log) == 2 && loads == 0);
    fw = 0x09600011;
    assert(bluehen_modules(log) == 0 && loads == 4 && unloads == 4 && lookups == 3);
    for (fail_load = 0; fail_load < 4; fail_load++) {
        loads = unloads = lookups = 0;
        assert(bluehen_modules(log) == 3);
        assert(unloads == fail_load && lookups == 0);
    }
    loads = unloads = lookups = 0; fail_load = -1; fail_unload = 0;
    assert(bluehen_modules(log) == 6 && unloads == 1);
    fclose(log);
    puts("module probe firmware, partial-load cleanup, reverse unload and stop-on-failure PASS");
}
